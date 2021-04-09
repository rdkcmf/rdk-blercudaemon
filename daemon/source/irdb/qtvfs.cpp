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
//  qtvfs.h
//  SkyBluetoothRcu
//

#include "qtvfs.h"

#include <QFile>
#include <QFileInfo>
#include <QResource>
#include <QDateTime>
#include <QDebug>
#include <QThread>

#include <string.h>
#include <sqlite3.h>


#ifndef SQLITE_DEFAULT_SECTOR_SIZE
#  define SQLITE_DEFAULT_SECTOR_SIZE 512
#endif




static int qtvfsFileClose(sqlite3_file *pFile);
static int qtvfsFileRead(sqlite3_file *pFile, void *pBuf, int amt,
                         sqlite3_int64 offset);
static int qtvfsFileWrite(sqlite3_file *, const void *, int, sqlite3_int64);
static int qtvfsFileTruncate(sqlite3_file *, sqlite3_int64);
static int qtvfsFileSync(sqlite3_file *, int flags);
static int qtvfsFileSize(sqlite3_file *pFile, sqlite3_int64 *pSize);
static int qtvfsFileLock(sqlite3_file *, int);
static int qtvfsFileUnlock(sqlite3_file *, int);
static int qtvfsFileCheckReservedLock(sqlite3_file *, int *pResOut);
static int qtvfsFileControl(sqlite3_file *, int, void *);
static int qtvfsFileSectorSize(sqlite3_file *);
static int qtvfsFileDeviceCharacteristics(sqlite3_file *);




// -----------------------------------------------------------------------------
/*!

 */
static const sqlite3_io_methods qtvfsFileMethods = {
	1,
	qtvfsFileClose,
	qtvfsFileRead,
	qtvfsFileWrite,
	qtvfsFileTruncate,
	qtvfsFileSync,
	qtvfsFileSize,
	qtvfsFileLock,
	qtvfsFileUnlock,
	qtvfsFileCheckReservedLock,
	qtvfsFileControl,
	qtvfsFileSectorSize,
	qtvfsFileDeviceCharacteristics,

	// methods below are valid for version 2 and above only
	nullptr,    // xShmMap
	nullptr,    // xShmLock
	nullptr,    // xShmBarrier
	nullptr,    // xShmUnmap

	// methods below are valid for version 3 and above only
	nullptr,    // xFetch
	nullptr,    // xUnfetch
};


// -----------------------------------------------------------------------------
/*!
	The qt_file structure is subclass of sqlite3_file specific to the Qt VFS
	implementations
 */
typedef struct qt_file qt_file;
struct qt_file
{
	const sqlite3_io_methods *pMethod; /*** Must be first ***/

#if defined(QTVFS_USE_QRESOURCE)

	QResource *qResource;

#else

	QFile *qFile;

#endif
};


// -----------------------------------------------------------------------------
/*!
	\reimp

	Opens an sqlite database file using Qt's QFile object.

 */
static int qtvfsOpen(sqlite3_vfs *pVfs, const char *zPath, sqlite3_file *pFile,
                     int flags, int *pOutFlags)
{
	Q_UNUSED(pVfs);

	qt_file *qtFile = reinterpret_cast<qt_file*>(pFile);
	if (Q_UNLIKELY(qtFile == nullptr)) {
		qWarning("invalid file struct");
		return SQLITE_NULL;
	}

	qDebug("attempting to open sqlite db file @ '%s'", zPath);


	// pMethod must be set to NULL, even if xOpen call fails.
	//
	// http://www.sqlite.org/c3ref/io_methods.html
	// "The only way to prevent a call to xClose following a failed sqlite3_vfs.xOpen
	// is for the sqlite3_vfs.xOpen to set the sqlite3_file.pMethods element to NULL."
	memset(qtFile, 0x00, sizeof(qt_file));


	// only allow opening a database in read-only mode
	if (!zPath || (flags & SQLITE_OPEN_DELETEONCLOSE) ||
	             !(flags & SQLITE_OPEN_READONLY) ||
	              (flags & SQLITE_OPEN_READWRITE) ||
	              (flags & SQLITE_OPEN_CREATE) ||
	             !(flags & SQLITE_OPEN_MAIN_DB)) {
		qWarning("cannot open read/write database with qtVFS backend");
		return SQLITE_PERM;
	}

#if defined(QTVFS_USE_QRESOURCE)

	// try and open the resource
	qtFile->qResource = new QResource(zPath);
	if (!qtFile->qResource->isValid() || (qtFile->qResource->data() == nullptr)) {
		qWarning() << "failed to open sqlite file @" << zPath
		           << "because is not present or not a file";

		delete qtFile->qResource;
		qtFile->qResource = nullptr;
		return SQLITE_CANTOPEN;
	}

	// we don't support compressed resources (hint use QMAKE_RESOURCE_FLAGS +=
	// -no-compress to disable resource compression)
	if (qtFile->qResource->isCompressed()) {
		qWarning() << "failed to open sqlite file @" << zPath
		           << "because is the resource is compressed";

		delete qtFile->qResource;
		qtFile->qResource = nullptr;
		return SQLITE_CANTOPEN;
	}

#else // !defined(QTVFS_USE_QRESOURCE)

	// try and open the file (if the caller prefixed with ':' then the file will
	// be opened from the built in resources)
	qtFile->qFile = new QFile(zPath);
	if (!qtFile->qFile->open(QFile::ReadOnly)) {
			qWarning() << "failed to open sqlite file @" << zPath
			           << "due to" << qtFile->qFile->errorString();

		delete qtFile->qFile;
		qtFile->qFile = nullptr;
		return SQLITE_CANTOPEN;
	}

#endif

	// set the methods to use on the file
	qtFile->pMethod = &qtvfsFileMethods;

	// return the flags used
	if (pOutFlags)
		*pOutFlags = flags;

	qDebug("opened sqlite db file @ '%s'", zPath);

	// success
	return SQLITE_OK;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Not implemented, we only support read-only databases.
 */
static int qtvfsDelete(sqlite3_vfs *pVfs, const char *zPath, int dirSync)
{
	Q_UNUSED(pVfs);
	Q_UNUSED(zPath);
	Q_UNUSED(dirSync);

	return SQLITE_ERROR;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Tests if file or resource exists and/or can be read.
 */
static int qtvfsAccess(sqlite3_vfs *pVfs, const char *zPath, int flags,
                       int *pResOut)
{
	Q_UNUSED(pVfs);

	*pResOut = 0;

	if (Q_UNLIKELY(!zPath))
		return SQLITE_ERROR;

#if defined(QTVFS_USE_QRESOURCE)

	switch (flags) {
		case SQLITE_ACCESS_EXISTS:
		case SQLITE_ACCESS_READ:
			{
				QResource resource(zPath);
				*pResOut = resource.isValid()&&
				           (resource.data() != nullptr) &&
				           !resource.isCompressed();
			}
			break;

		default:
			break;
	}

#else // !defined(QTVFS_USE_QRESOURCE)

	switch (flags) {
		case SQLITE_ACCESS_EXISTS:
			*pResOut = QFile::exists(zPath) ? 1 : 0;
			break;

		case SQLITE_ACCESS_READ:
			{
				QFile file(zPath);
				if (file.open(QFile::ReadOnly) == true) {
					file.close();
					*pResOut = 1;
				}
			}
			break;

		default:
			// all other access denied
			break;
	}

#endif // !defined(QTVFS_USE_QRESOURCE)

	return SQLITE_OK;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Returns the fully qualified path of the file.
 */
static int qtvfsFullPathname(sqlite3_vfs *pVfs, const char *zPath, int nOut,
                             char *zOut)
{
	Q_UNUSED(pVfs);

	if (Q_UNLIKELY(!zPath))
		return SQLITE_ERROR;

	const QFileInfo info(zPath);
	const QString path = info.canonicalFilePath();

	if (path.length() >= nOut)
		return SQLITE_ERROR;

	strncpy(zOut, qPrintable(path), nOut);
	zOut[nOut - 1] = '\0';

	return SQLITE_OK;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Fills the buffer with random data.
 */
static int qtvfsRandomness(sqlite3_vfs *pVfs, int nBuf, char *zBuf)
{
	Q_UNUSED(pVfs);

	for (int i = 0; i < nBuf; i++)
		zBuf[i] = static_cast<char>(qrand() & 0xff);

	return nBuf;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

 */
static int qtvfsSleep(sqlite3_vfs *pVfs, int microseconds)
{
	Q_UNUSED(pVfs);

	QThread::msleep(microseconds);
	return microseconds;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Find the current time (in Universal Coordinated Time).  Write into *piNow
	the current time and date as a Julian Day number times 86_400_000.  In
	other words, write into *piNow the number of milliseconds since the Julian
	epoch of noon in Greenwich on November 24, 4714 B.C according to the
	proleptic Gregorian calendar.

	On success, return SQLITE_OK.  Return SQLITE_ERROR if the time and date
	cannot be found.

 */
static int qtvfsCurrentTimeInt64(sqlite3_vfs *pVfs, sqlite3_int64 *piNow)
{
	Q_UNUSED(pVfs);

	static const qint64 unixEpoch = 24405875LL * 8640000LL;

	*piNow = unixEpoch + QDateTime::currentMSecsSinceEpoch();

	return SQLITE_OK;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Find the current time (in Universal Coordinated Time).  Write the
	current time and date as a Julian Day number into *prNow and
	return 0.  Return 1 if the time and date cannot be found.

 */
static int qtvfsCurrentTime(sqlite3_vfs *pVfs, double *prNow)
{
	sqlite3_int64 msecsNow;

	int rc = qtvfsCurrentTimeInt64(pVfs, &msecsNow);
	*prNow = double(msecsNow) / 86400000.0f;
	return rc;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Not implemented (no additional information)

 */
static int qtvfsGetLastError(sqlite3_vfs *pVfs, int NotUsed2, char *NotUsed3)
{
	Q_UNUSED(pVfs);
	Q_UNUSED(NotUsed2);
	Q_UNUSED(NotUsed3);

	return 0;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Closing file opened in sqlite3_vfs.xOpen function.

 */
static int qtvfsFileClose(sqlite3_file *pFile)
{
	qt_file *qtFile = reinterpret_cast<qt_file*>(pFile);

#if defined(QTVFS_USE_QRESOURCE)

	delete qtFile->qResource;
	qtFile->qResource = nullptr;

#else // !defined(QTVFS_USE_QRESOURCE)

	if (qtFile->qFile) {
		qtFile->qFile->flush();
		qtFile->qFile->close();

		delete qtFile->qFile;
		qtFile->qFile = nullptr;
	}

#endif

	return SQLITE_OK;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Database read from QFile.

 */
static int qtvfsFileRead(sqlite3_file *pFile, void *pBuf, int amt,
                         sqlite3_int64 offset)
{
	qt_file *qtFile = reinterpret_cast<qt_file*>(pFile);
	qint64 amount = amt;
	qint64 avail;
	int rc;

#if defined(QTVFS_USE_QRESOURCE)

	// sanity check
	if (Q_UNLIKELY(!qtFile || (qtFile->qResource == nullptr)))
		return SQLITE_IOERR_READ;

	//
	const qint64 resourceSize = qtFile->qResource->size();
	if ((offset + amount) <= resourceSize) {
		avail = amount;
		rc = SQLITE_OK;

	} else {
		avail = resourceSize - offset;
		if (avail < 0) {
			rc = SQLITE_IOERR_READ;

		} else {
			// http://www.sqlite.org/c3ref/io_methods.html
			// "If xRead() returns SQLITE_IOERR_SHORT_READ it must also
			// fill in the unread portions of the buffer with zeros.
			// A VFS that fails to zero-fill short reads might seem to work.
			// However, failure to zero-fill short reads will eventually lead
			// to database corruption."
			//
			// It might be not a problem in read-only databases,
			// but do it as documentation says
			memset(reinterpret_cast<quint8*>(pBuf) + avail, 0x00, (amount - avail));
			rc = SQLITE_IOERR_SHORT_READ;
		}
	}

	if (avail > 0) {
		const uint8_t *data = qtFile->qResource->data() + offset;
		memcpy(pBuf, data, avail);
	}

#else // !defined(QTVFS_USE_QRESOURCE)

	// sanity check
	if (Q_UNLIKELY(!qtFile || (qtFile->qFile == nullptr)))
		return SQLITE_IOERR_READ;

	//
	const qint64 fileSize = qtFile->qFile->size();
	if ((offset + amount) <= fileSize) {
		avail = amount;
		rc = SQLITE_OK;

	} else {
		avail = fileSize - offset;
		if (avail < 0) {
			rc = SQLITE_IOERR_READ;

		} else {
			// http://www.sqlite.org/c3ref/io_methods.html
			// "If xRead() returns SQLITE_IOERR_SHORT_READ it must also
			// fill in the unread portions of the buffer with zeros.
			// A VFS that fails to zero-fill short reads might seem to work.
			// However, failure to zero-fill short reads will eventually lead
			// to database corruption."
			//
			// It might be not a problem in read-only databases,
			// but do it as documentation says
			memset(reinterpret_cast<quint8*>(pBuf) + avail, 0x00, (amount - avail));
			rc = SQLITE_IOERR_SHORT_READ;
		}
	}

	if (avail > 0) {
		if (qtFile->qFile->seek(offset) == false) {
			qWarning("failed to seek to offset");
			rc = SQLITE_IOERR_READ;

		} else if (qtFile->qFile->read(static_cast<char*>(pBuf), avail) != avail) {
			qWarning("failed to read the complete available bytes");
			rc = SQLITE_IOERR_READ;
		}
	}

#endif // !defined(QTVFS_USE_QRESOURCE)

	return rc;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Not implemented only support read-only.

 */
static int qtvfsFileWrite(sqlite3_file *, const void *, int, sqlite3_int64)
{
	return SQLITE_IOERR_WRITE;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Not implemented only support read-only.

 */
static int qtvfsFileTruncate(sqlite3_file *, sqlite3_int64)
{
	return SQLITE_IOERR_TRUNCATE;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Not implemented only support read-only.

 */
static int qtvfsFileSync(sqlite3_file *, int)
{
	return SQLITE_IOERR_FSYNC;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Gets the size of the database file.

 */
static int qtvfsFileSize(sqlite3_file *pFile, sqlite3_int64 *pSize)
{
	qt_file *qtFile = reinterpret_cast<qt_file*>(pFile);
	if (Q_UNLIKELY(!qtFile))
		return SQLITE_ERROR;

#if defined(QTVFS_USE_QRESOURCE)

	if (Q_UNLIKELY(qtFile->qResource == nullptr))
		return SQLITE_ERROR;

	*pSize = qtFile->qResource->size();

#else // !defined(QTVFS_USE_QRESOURCE)

	if (Q_UNLIKELY(qtFile->qFile == nullptr))
		return SQLITE_ERROR;

	*pSize = qtFile->qFile->size();

#endif // !defined(QTVFS_USE_QRESOURCE)

	return SQLITE_OK;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Not implemented only support read-only.

 */
static int qtvfsFileLock(sqlite3_file *, int)
{
	return SQLITE_OK;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Not implemented only support read-only.

 */
static int qtvfsFileUnlock(sqlite3_file *, int)
{
	return SQLITE_OK;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Not implemented only support read-only.

 */
static int qtvfsFileCheckReservedLock(sqlite3_file *, int *pResOut)
{
	*pResOut = 0;
	return SQLITE_OK;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Not implemented (no special codes needed for now).

 */
static int qtvfsFileControl(sqlite3_file *, int, void *)
{
	return SQLITE_NOTFOUND;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Use same value as in os_unix.c

 */
static int qtvfsFileSectorSize(sqlite3_file *)
{
	return SQLITE_DEFAULT_SECTOR_SIZE;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Not implemented only support read-only.

 */
static int qtvfsFileDeviceCharacteristics(sqlite3_file *)
{
	return 0;
}


// -----------------------------------------------------------------------------
/*!
	Registers the Qt QFile based VFS implementation for reading sqlite database
	files from resources.

 */
int qtvfsRegister(bool makeDefault)
{
	static bool isRegistered = false;
	static char qtVfsName[] = SQLITE_QT_VFS_NAME;
	static sqlite3_vfs qtVfs;

	// check we're not already registered
	if (isRegistered) {
		qWarning("already registered 'qt-vfs' sqlite backend");
		return SQLITE_ERROR;
	}


	// populate the structure
	bzero(&qtVfs, sizeof(sqlite3_vfs));

	qtVfs.iVersion = 3;
	qtVfs.szOsFile = sizeof(qt_file);
	qtVfs.mxPathname = 512;
	qtVfs.pNext = nullptr;
	qtVfs.zName = qtVfsName;
	qtVfs.pAppData = nullptr;
	qtVfs.xOpen = qtvfsOpen;
	qtVfs.xDelete = qtvfsDelete;
	qtVfs.xAccess = qtvfsAccess;
	qtVfs.xFullPathname = qtvfsFullPathname;
	qtVfs.xDlOpen = nullptr;
	qtVfs.xDlError = nullptr;
	qtVfs.xDlSym = nullptr;
	qtVfs.xDlClose = nullptr;
	qtVfs.xRandomness = qtvfsRandomness;
	qtVfs.xSleep = qtvfsSleep;
	qtVfs.xCurrentTime = qtvfsCurrentTime;
	qtVfs.xGetLastError = qtvfsGetLastError;
	qtVfs.xCurrentTimeInt64 = qtvfsCurrentTimeInt64;
	qtVfs.xSetSystemCall = nullptr;
	qtVfs.xGetSystemCall = nullptr;
	qtVfs.xNextSystemCall = nullptr;


	// try to register VFS
	int rc = sqlite3_vfs_register(&qtVfs, makeDefault ? 1 : 0);
	if (rc != SQLITE_OK) {
		qWarning("failed to register Qt VFS (%d - %s)", rc, sqlite3_errstr(rc));

	} else {
		qDebug("registered sqlite VFS for QT");
		isRegistered = true;
	}

	return rc;
}
