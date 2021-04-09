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
//  fwimagefile.h
//  SkyBluetoothRcu
//

#ifndef FWIMAGEFILE_H
#define FWIMAGEFILE_H

#include <QString>
#include <QScopedPointer>
#include <QIODevice>
#include <QVersionNumber>


class FwImageFile
{
public:
	explicit FwImageFile(const QByteArray &data);
	explicit FwImageFile(const QString &filePath);
	explicit FwImageFile(int fd);
	~FwImageFile();

public:
	bool isValid() const;

	QString errorString() const;

	quint8 manufacturerId() const;
	QVersionNumber hwVersion() const;
	QVersionNumber version() const;

	quint32 crc32() const;

public:
	bool atEnd() const;
	qint64 pos() const;
	bool seek(qint64 pos);

	qint64 size() const;

	qint64 read(void *data, qint64 len);

private:
	bool checkFile();

private:
	QScopedPointer<QIODevice> m_file;
	QString m_error;

	bool m_valid;
	quint32 m_hardwareVersion = 0;
	quint32 m_firmwareVersion = 0;
	quint32 m_firmwareSize = 0;
	quint32 m_firmwareCrc = 0;

private:
	Q_DISABLE_COPY(FwImageFile)
};

#endif // !defined(FWIMAGEFILE_H)
