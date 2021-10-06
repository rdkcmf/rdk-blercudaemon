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
//  blercuaudioservice.h
//  SkyBluetoothRcu
//

#ifndef BLERCUAUDIOSERVICE_H
#define BLERCUAUDIOSERVICE_H

#include "utils/future.h"
#include "utils/filedescriptor.h"

#include <QObject>
#include <QString>
#include <QSharedPointer>


class BleRcuAudioService : public QObject
{
	Q_OBJECT

protected:
	explicit BleRcuAudioService(QObject *parent)
		: QObject(parent)
	{ }
	
public:
	~BleRcuAudioService() override = default;

public:
	enum Encoding {
		ADPCM,
		PCM16,
#ifndef Q_QDOC
		InvalidEncoding
#endif
	};
#if QT_VERSION > QT_VERSION_CHECK(5, 4, 0)
	Q_ENUM(Encoding)
#else
	Q_ENUMS(Encoding)
#endif

public:
	virtual bool isStreaming() const = 0;

	virtual quint8 gainLevel() const = 0;
	virtual void setGainLevel(quint8 level) = 0;

	virtual quint32 audioCodecs() const = 0;

	virtual Future<FileDescriptor> startStreaming(Encoding encoding) = 0;
	virtual Future<> startStreamingTo(Encoding encoding, int pipeWriteFd) = 0;
	virtual Future<> stopStreaming() = 0;

public:
	enum StreamingError {
		NoError = 0,
		DeviceDisconnectedError = 1,
		InternalError = 2,
	};

	struct StatusInfo {
		quint32 lastError;
		quint32 expectedPackets;
		quint32 actualPackets;
	};

	virtual Future<StatusInfo> status() = 0;

signals:
	void streamingChanged(bool streamming);
	void gainLevelChanged(quint8 level);
	void audioCodecsChanged(quint32 codecs);
};

Q_DECLARE_METATYPE(BleRcuAudioService::StatusInfo)


#endif // !defined(BLERCUAUDIOSERVICE_H)
