/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2017-2020 Sky UK
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

//
//  irdatabase.cpp
//  SkyBluetoothRcu
//

#include "irdatabase_p.h"
#include "qtvfs.h"
#include "utils/logging.h"
#include "utils/edid.h"

#include <cctype>




// -----------------------------------------------------------------------------
/*!
	Creates a new IrDatabase object for the sqlite database file supplied in
	the \a dbPath argument.

	If the db file doesn't exist or has incompatible schema then an empty
	shared pointer is returned.

 */
QSharedPointer<IrDatabase> IrDatabase::create(const QString &dbPath)
{
	QSharedPointer<IrDatabase> db = QSharedPointer<IrDatabaseImpl>::create(dbPath);
	if (!db || !db->isValid())
		db.reset();

	return db;
}


IrDatabaseImpl::IrDatabaseImpl(const QString &dbPath)
	: m_sqliteDB(nullptr)
	, m_tvBrandsCache(10)
	, m_ampBrandsCache(10)
{
	init(dbPath);
}

IrDatabaseImpl::~IrDatabaseImpl()
{
	if (m_sqliteDB) {
		sqlite3_close(m_sqliteDB);
		m_sqliteDB = nullptr;
	}

}

// -----------------------------------------------------------------------------
/*!
	\internal

	Normalises the \a string by stripping all non-alphanumeric characters and
	converting to all upper case.  Some basic localisation is performed to
	convert a limited set of accented characters to their englishised (is that
	a word?) form.

	The normalised string is copied into \a buffer, the output string will be
	clipped to \a bufferLen and is guaranteed to be null terminated.

 */
static size_t normaliseString(char *buffer, size_t bufferLen, const QString &string)
{
	char *bufferPtr = buffer;
	char *bufferLast = buffer + (bufferLen - 1);

	for (const QChar &ch : string) {

		// check we've hit the end of the buffer
		if (bufferPtr == bufferLast)
			break;

		// convert the character to unicode
		ushort ch_ = ch.unicode();

		if (ch_ < 128) {

			// is a good old ascii value, if it's alpha numeric then add to
			// the output
			if (std::isalnum(ch_))
				*bufferPtr++ = static_cast<char>(std::toupper(ch_));

		} else {

			// unicode value, check if we can convert it
			switch (ch_) {
				case u'à': case u'á': case u'ä': case u'â': case u'ã': case u'å':
				case u'À': case u'Á': case u'Ä': case u'Â': case u'Ã': case u'Å':
					*bufferPtr++ = 'A';
					break;
				case u'é': case u'è': case u'ê': case u'ë':
				case u'È': case u'É': case u'Ê': case u'Ë':
					*bufferPtr++ = 'E';
					break;
				case u'ì': case u'í': case u'î': case u'ï':
				case u'Ì': case u'Í': case u'Î': case u'Ï':
					*bufferPtr++ = 'I';
					break;
				case u'ò': case u'ó': case u'ô': case u'õ': case u'ö': case u'ø':
				case u'Ò': case u'Ó': case u'Ô': case u'Õ': case u'Ö': case u'Ø':
					*bufferPtr++ = 'O';
					break;
				case u'ù': case u'ú': case u'û': case u'ü':
				case u'Ù': case u'Ú': case u'Û': case u'Ü':
					*bufferPtr++ = 'U';
					break;
				case u'ß':
					*bufferPtr++ = 'B';
					break;
				case u'Ñ': case u'ñ':
					*bufferPtr++ = 'N';
					break;

				default:
					break;
			}
		}
	}

	*bufferPtr = '\0';

	return (bufferPtr - buffer);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	SQLite3 collation function used for the string comparision used in the
	lookup of the make and models.

	This performs a comparision using all upper case, with non-alpha numeric
	characters removed and european characters simplified.

 */
static int sqliteSimpleCompare(void* arg, int len1, const void* data1,
                               int len2, const void* data2)
{
	Q_UNUSED(arg);

	const QString string1 = QString::fromRawData(reinterpret_cast<const QChar*>(data1),
	                                             (len1 / sizeof(QChar)));
	const QString string2 = QString::fromRawData(reinterpret_cast<const QChar*>(data2),
	                                             (len2 / sizeof(QChar)));

	char buffer1[96];
	char buffer2[96];

	normaliseString(buffer1, sizeof(buffer1), string1);
	normaliseString(buffer2, sizeof(buffer2), string2);

	return strcmp(buffer1, buffer2);
}


// -----------------------------------------------------------------------------
/*!
	\internal

	Implements a custom LIKE comparer for the IR database.  It is case
	in-sensitive and removes all punctuation as well as some magic source to
	handle a limited number of non-ascii characters.

 */
static void sqliteSimpleLikeFunc(sqlite3_context *context,
                                 int argc, sqlite3_value **argv)
{
	// sanity check that the ESCAPE clause wasn't added
	if (Q_UNLIKELY(argc == 3)) {
		sqlite3_result_error(context, "LIKE pattern doesn't support ESCAPE", -1);
		return;
	}

	// to speed things up the pattern part of the LIKE should have already been
	// normalised so no need to do it again
	const char *pattern = reinterpret_cast<const char*>(sqlite3_value_text(argv[0]));
	int patternLen = sqlite3_value_bytes(argv[0]);

	const char *string_ = reinterpret_cast<const char*>(sqlite3_value_text(argv[1]));

	if (Q_UNLIKELY(!pattern || !string_ || (patternLen < 0))) {
		sqlite3_result_error(context, "LIKE function received invalid pattern or sting ", -1);
		return;
	}

	// however the value we're comparing against does need to be normalised
	char string[96];
	normaliseString(string, sizeof(string), QString::fromUtf8(string_));


	// we always do a prefix search, so don't worry about checking for trailing
	// match all characters
	int result = strncmp(pattern, string, patternLen);

	sqlite3_result_int(context, (result == 0));
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Re-implementation of IrDatabaseImpl::normaliseString for QStrings rather than
	raw string data.

 */
QString IrDatabaseImpl::normaliseString(const QString &string) const
{
	char buf[96];

	size_t len = ::normaliseString(buf, sizeof(buf), string);

	return QLatin1String(buf, static_cast<int>(len));
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Opens the SQLite database for reading.

 */
void IrDatabaseImpl::init(const QString &dbPath)
{
	// initialize sqlite3's global variables
	sqlite3_initialize();

	// initialise the Qt VFS backend so we can read the database from qt resources
	QString vfsBackend;
	if (dbPath.startsWith(':')) {
		qtvfsRegister();
		vfsBackend = SQLITE_QT_VFS_NAME;
	}

	qDebug("attempting to open sqlite db @ '%s' with vfs backend of '%s'",
	       qPrintable(dbPath), qPrintable(vfsBackend));


	// attempt to open the database file
	int res = sqlite3_open_v2(qPrintable(dbPath), &m_sqliteDB,
	                          SQLITE_OPEN_READONLY, qPrintable(vfsBackend));

	if (res != SQLITE_OK) {
		qError("unable to open database (%d - %s)", res, sqlite3_errstr(res));
		m_sqliteDB = nullptr;
		return;
	}

	qInfo("opened ir database @ '%s'", qPrintable(dbPath));


	// add the collation used for searching
	res = sqlite3_create_collation(m_sqliteDB, "SKY_NOCASE", SQLITE_UTF16,
	                               nullptr, sqliteSimpleCompare);
	if (res != SQLITE_OK) {
		qError("failed to install collating function (%d - %s)",
		       res, sqlite3_errstr(res));
	}


	// add our own simple LIKE function that replaces the standard SQLite
	// version, our one is case insensitive and removes all punctuation.
	res = sqlite3_create_function(m_sqliteDB, "LIKE", 2,
	                              SQLITE_UTF8 | SQLITE_DETERMINISTIC,
	                              nullptr, sqliteSimpleLikeFunc, nullptr, nullptr);
	if (res != SQLITE_OK) {
		qError("failed to install LIKE function (%d - %s)",
		       res, sqlite3_errstr(res));
	}

}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the internal database is valid and open.

 */
bool IrDatabaseImpl::isValid() const
{
	return (m_sqliteDB != nullptr);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Performs the query using the supplied \a select statement.  If the optional
	\a total argument is not \c nullptr then we first get the total number
	of results for the given SELECT and put the result back in \a total.

	The \a offset and \a limit arguments are also optional, if positive values
	then the LIMIT and OFFSET SQL parameters will be added to the SELECT.

 */
QVariantList IrDatabaseImpl::execQuery(const QString &query,
                                       const QVariantList &params,
                                       quint64 *total, qint64 offset, qint64 limit) const
{

	// set the start offset to zero if not supplied
	if (offset < 0)
		offset = 0;
	if (limit < 0)
		limit = INT32_MAX;


	sqlite3_stmt *stmt;

	// prepare the query
#if (SQLITE_VERSION_NUMBER >= 3003011)
	int res = sqlite3_prepare16_v2(m_sqliteDB, query.constData(),
	                               ((query.size() + 1) * sizeof(QChar)),
	                               &stmt, nullptr);
#else
	int res = sqlite3_prepare16(m_sqliteDB, query.constData(),
	                            ((query.size() + 1) * sizeof(QChar)),
	                            &stmt, nullptr);
#endif
	if (res != SQLITE_OK) {
		qError("failed to prepare query (%d - %s)", res, sqlite3_errstr(res));
		return QVariantList();
	}

	// bind any parameters
	int nParam = 1;
	for (const QVariant &param : params) {

		switch (param.type()) {
			case QVariant::Double:
				res = sqlite3_bind_double(stmt, nParam, param.value<double>());
				break;
			case QVariant::Int:
			case QVariant::UInt:
				res = sqlite3_bind_int(stmt, nParam, param.value<int>());
				break;
			case QVariant::LongLong:
			case QVariant::ULongLong:
				res = sqlite3_bind_int64(stmt, nParam, param.value<qint64>());
				break;
			case QVariant::ByteArray:
			{
				const QByteArray value = param.value<QByteArray>();
				res = sqlite3_bind_blob(stmt, nParam, value.constData(),
				                        value.length(), SQLITE_TRANSIENT);
				break;
			}
			case QVariant::String:
			{
				const QString value = param.value<QString>();
				res = sqlite3_bind_text16(stmt, nParam, value.constData(),
				                          (value.length() * sizeof(QChar)),
				                          SQLITE_TRANSIENT);
				break;
			}

			default:
				qWarning("sql parameter @ index %d is non-supported type,"
				         " ignoring", nParam);
				res = SQLITE_OK;
				break;
		}

		if (res != SQLITE_OK) {
			qError("failed to bind parameter @ index %d (%d - %s)", nParam,
			       res, sqlite3_errstr(res));
			return QVariantList();
		}

		nParam++;
	}


	//
	QVariantList results;

	// we only need column 0 of the results
	const int nColumn = 0;

	// step over the results
	qint64 nRow = 0;
	while ((res = sqlite3_step(stmt)) == SQLITE_ROW) {

		// check the row is within the offset and range
		if ((nRow >= offset) && (nRow < (offset + limit))) {

			// convert the type to a QVariant
			switch (sqlite3_column_type(stmt, nColumn)) {
				case SQLITE_BLOB:
					results.append( QByteArray(static_cast<const char *>(sqlite3_column_blob(stmt, nColumn)),
					                           sqlite3_column_bytes(stmt, nColumn)) );
					break;
				case SQLITE_INTEGER:
					results.append(sqlite3_column_int64(stmt, nColumn));
					break;
				case SQLITE_FLOAT:
					results.append(sqlite3_column_double(stmt, nColumn));
					break;
				case SQLITE_NULL:
					break;
				default:
					results.append( QString(reinterpret_cast<const QChar*>(sqlite3_column_text16(stmt, nColumn)),
					                        sqlite3_column_bytes16(stmt, nColumn) / sizeof(QChar)) );
					break;
			}

		}

		// skip out if exceeded range and we don't care about the count
		if (Q_UNLIKELY((total == nullptr) && (nRow >= (offset + limit))))
			break;

		nRow++;
	}

	// check for a step error
	if (res != SQLITE_DONE)
		qError("step through query results (%d - %s)", res, sqlite3_errstr(res));

	// finalise the query
	res = sqlite3_finalize(stmt);
	if (res != SQLITE_OK)
		qError("failed to finalise query (%d - %s)", res, sqlite3_errstr(res));

	// store the total possible results
	if (total != nullptr)
		*total = static_cast<quint64>(nRow);

	return results;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Returns the id for the given \a brand and \a type.

 */
int IrDatabaseImpl::getBrandId(const QString &brand, Type type) const
{
	// first check the cache
	int *cachedBrandId = nullptr;
	if (type == Televisions)
		cachedBrandId = m_tvBrandsCache.object(brand);
	else if (type == AVAmplifiers)
		cachedBrandId = m_ampBrandsCache.object(brand);
	else
		return INT_MIN;

	if (cachedBrandId != nullptr)
		return *cachedBrandId;


	// get the type id
	int typeId = (type == Televisions) ? 1 : 2;

	// construct the SELECT query
	static const QString select("SELECT brands.brand_id"
	                            "  FROM brands"
	                            "    WHERE brands.type=?1 AND brands.name=?2");

	// execute the query
	const QVariantList results = execQuery(select, { typeId, brand },
	                                       nullptr, -1, -1);

	// process the results
	if (results.isEmpty())
		return INT_MIN;

	const QVariant &result = results.at(0);
	if (!result.canConvert<int>()) {
		qWarning("brands.id result is not an integer?");
		return INT_MIN;
	}

	int brandId = result.toInt();

	// add to the cache
	if (type == Televisions)
		m_tvBrandsCache.insert(brand, new int(brandId));
	else
		m_ampBrandsCache.insert(brand, new int(brandId));

	return brandId;
}

// -----------------------------------------------------------------------------
/*!
	

 */
QStringList IrDatabaseImpl::brands(Type type, const QString &search,
                                   quint64 *total, qint64 offset, qint64 limit) const
{
	// normalise the search string (strip whitespace and captialises)
	const QString brand = normaliseString(search);

	// sanity check the limit value, it is not allowed to be zero
	if (Q_UNLIKELY(limit == 0)) {
		qWarning("limit argument cannot be zero");
		return QStringList();
	}

	// get the type id
	int typeId = (type == Televisions) ? 1 : 2;

	// construct the SELECT query
	static const QString select("SELECT brands.name"
	                            "  FROM brands"
	                            "    WHERE brands.type=?1 AND brands.name LIKE ?2"
	                            "    ORDER BY brands.name COLLATE SKY_NOCASE");

	// execute the query
	const QVariantList results = execQuery(select, { typeId, brand },
	                                       total, offset, limit);
	if (results.isEmpty()) {
		qInfo("no matching brands or failed to execute query");
		return QStringList();
	}

	// iterate through the results
	QStringList brands;
	for (const QVariant &result : results) {

		if (!result.canConvert<QString>())
			qWarning("cannot convert result to string");
		else
			brands.append(result.toString());
	}

	return brands;
}

// -----------------------------------------------------------------------------
/*!
	Performs a lookup of a model using the \a search string. The \a search
	string is first normalised to remove all whitespace and punctuation
	characters and then a limited locale transform is done to convert to
	the simplified Latin1 character set.  This is then used in the comparision
	to the model names in the database.

	The \a brand string must match exactly match a brand returned in the
	previous lookup using the IrDatabase::brand() method.  THe \a type
	must be either IrDatabase::Television or IrDatabase::AVAmplifier.

	If \a total is not null, then the total number of matches found is returned
	in the value.  Both \a offset and \a limit are optional, if specified they
	define the the offset of the first record to return and limit defines the
	maximum number of results to return.

 */
QStringList IrDatabaseImpl::models(Type type, const QString &brand,
                                   const QString &search, quint64 *total,
                                   qint64 offset, qint64 limit) const
{
	// sanity check the limit value, it is not allowed to be zero
	if (Q_UNLIKELY(limit == 0)) {
		qWarning("limit argument cannot be zero");
		return QStringList();
	}

	// try and get the brand.id first, these are cached so potentially faster
	// than performing a full sql query
	int brandId = getBrandId(brand, type);
	if (brandId == INT_MIN) {
		qDebug() << "no brand with name" << brand;
		return QStringList();
	}

	// normalise the search string (strip whitespace and captialises)
	const QString model = normaliseString(search);

	// construct the SELECT query and execute it
	QVariantList results;
	if (model.isEmpty()) {
		static const QString select("SELECT models.name"
		                            "  FROM models"
		                            "    WHERE models.brand_id=?1"
		                            "  ORDER BY models.name COLLATE SKY_NOCASE");
		results = execQuery(select, { brandId }, total, offset, limit);

	} else {
		static const QString select("SELECT models.name"
		                            "  FROM models"
		                            "    WHERE models.brand_id=?1 AND models.name LIKE ?2"
		                            "  ORDER BY models.name COLLATE SKY_NOCASE");
		results = execQuery(select, { brandId, model }, total, offset, limit);

	}

	// check the results
	if (results.isEmpty()) {
		qInfo("no matching models or failed to execute query");
		return QStringList();
	}

	// iterate through the results
	QStringList models;
	for (const QVariant &result : results) {

		if (!result.canConvert<QString>())
			qWarning("cannot convert result to string");
		else
			models.append(result.toString());
	}

	return models;
}

// -----------------------------------------------------------------------------
/*!
	Returns the list of code ids that match the \a brand, \a type and optionally
	the \a model.  If the \a model string is null or empty then all the codes
	for the given \a brand are returned.

	The \a brand must be an exact match to a value returned by the
	IrDatabase::brands() method.

 */
QList<int> IrDatabaseImpl::codeIds(Type type, const QString &brand,
                                   const QString &model) const
{

	// try and get the brand.id first, these are cached so potentially faster
	// than performing a full sql query
	int brandId = getBrandId(brand, type);
	if (brandId == INT_MIN) {
		qDebug() << "no brand with name" << brand;
		return QList<int>();
	}

	QVariantList results;

	// execute the query
	if (model.isEmpty()) {
		// if no model selected then get all unique codeIds for the given brand
		static const QString select("SELECT DISTINCT codeid_lookup.code_id"
		                            "  FROM codeid_lookup"
		                            "    WHERE codeid_lookup.brand_id=?1"
		                            "  ORDER BY codeid_lookup.ranking ASC");

		results = execQuery(select, { brandId }, nullptr, -1, -1);

	} else {
		// return all codeids that match the model name
		static const QString select("SELECT DISTINCT codeid_lookup.code_id"
		                            "  FROM codeid_lookup"
		                            "    WHERE codeid_lookup.brand_id=?1"
		                            "      AND (codeid_lookup.model_id IN (SELECT models.model_id"
		                            "                                      FROM models"
		                            "                                      WHERE models.brand_id=?1 AND models.name=?2))"
		                            "  ORDER BY codeid_lookup.ranking ASC");

		results = execQuery(select, { brandId, model }, nullptr, -1, -1);
	}

	if (results.isEmpty()) {
		qInfo("no matching brands / models or failed to execute query");
		return QList<int>();
	}

	// iterate through the results
	QList<int> codeIds;
	for (const QVariant &result : results) {

		if (!result.canConvert<int>())
			qWarning("cannot convert result to int");
		else
			codeIds.append(result.toInt());
	}

	return codeIds;
}

// -----------------------------------------------------------------------------
/*!
	\fn IrSignalSet IrDatabase::codeIds(const Edid &edid) const

	Returns a list of code ids that could match the given EDID.  Currently only
	the pnpId (manufacturer id) is used in the lookup.

 */
QList<int> IrDatabaseImpl::codeIds(const Edid &edid) const
{
	// sanity check the EDID
	if (!edid.isValid()) {
		qWarning("invalid edid");
		return QList<int>();
	}

	// construct the SELECT query
	static const QString select("SELECT DISTINCT edid_codeid.code_id"
	                            "  FROM edid_codeid"
	                            "    WHERE edid_codeid.edid_manuf_id=?1"
	                            "  ORDER BY edid_codeid.ranking ASC");

	// execute the query
	const QVariantList results = execQuery(select, { int(edid.pnpId()) },
	                                       nullptr, -1, -1);
	if (results.isEmpty()) {
		qInfo("no matching codes for EDID.manuf_id '%s'",
		      qPrintable(edid.manufacturerId()));
	}

	// iterate through the results
	QList<int> codeIds;
	for (const QVariant &result : results) {

		if (!result.canConvert<int>())
			qWarning("cannot convert result to int");
		else
			codeIds.append(result.toInt());
	}

	return codeIds;
}

// -----------------------------------------------------------------------------
/*!
	\fn IrSignalSet IrDatabase::irSignals(int codeId) const

	Gets the signal set for the given \a codeId


 */
IrSignalSet IrDatabaseImpl::irSignals(RcuType rcuType, int codeId) const
{
	Q_UNUSED(rcuType)

	// construct the SELECT query
	static const QString select("SELECT infrared_data.button_id, infrared_data.data"
	                            "  FROM infrared_data"
	                            "    WHERE infrared_data.code_id=?1");

	// execute the query
	sqlite3_stmt *stmt;

	// construct the query
#if (SQLITE_VERSION_NUMBER >= 3003011)
	int res = sqlite3_prepare16_v2(m_sqliteDB, select.constData(),
	                               ((select.size() + 1) * sizeof(QChar)),
	                               &stmt, nullptr);
#else
	int res = sqlite3_prepare16(m_sqliteDB, select.constData(),
	                            ((select.size() + 1) * sizeof(QChar)),
	                            &stmt, nullptr);
#endif
	if (res != SQLITE_OK) {
		qError("failed to perform select query (%d - %s)", res, sqlite3_errstr(res));
		return IrSignalSet();
	}

	// bind the codeId param
	res = sqlite3_bind_int(stmt, 1, codeId);
	if (res != SQLITE_OK) {
		qError("failed to bind param to query (%d - %s)", res, sqlite3_errstr(res));
		return IrSignalSet();
	}


	// step over the results
	IrSignalSet signalSet(codeId);
	while (sqlite3_step(stmt) == SQLITE_ROW) {

		// get the button id
		int buttonId = sqlite3_column_int(stmt, 0);

		// get the signal data
		QByteArray data(static_cast<const char *>(sqlite3_column_blob(stmt, 1)),
		                sqlite3_column_bytes(stmt, 1));


		// convert the signal button id to a local type
		Qt::Key key = Qt::Key_unknown;
		switch (buttonId) {
			case 12:  key = Qt::Key_Standby;      break;
			case 41:  key = Qt::Key_Settings;     break;   // input select
			case 16:  key = Qt::Key_VolumeUp;     break;
			case 17:  key = Qt::Key_VolumeDown;   break;
			case 13:  key = Qt::Key_VolumeMute;   break;
			case 92:  key = Qt::Key_Select;       break;
			case 88:  key = Qt::Key_Up;           break;
			case 90:  key = Qt::Key_Left;         break;
			case 91:  key = Qt::Key_Right;        break;
			case 89:  key = Qt::Key_Down;         break;
			default:
				qWarning("unknown button id %d", buttonId);
				break;
		}

		// add to the signal set if valid
		if (key != Qt::Key_unknown)
			signalSet.insert(key, std::move(data));
	}

	// finalise the query
	res = sqlite3_finalize(stmt);
	if (res != SQLITE_OK)
		qError("failed to finalise query (%d - %s)", res, sqlite3_errstr(res));


	return signalSet;
}

