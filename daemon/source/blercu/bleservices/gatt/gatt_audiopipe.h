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
//  gatt_audiopipe.h
//  SkyBluetoothRcu
//

#ifndef GATT_AUDIOPIPE_H
#define GATT_AUDIOPIPE_H

#include "utils/filedescriptor.h"

#include <QObject>
#include <QString>
#include <QElapsedTimer>
#include <QSharedPointer>


class VoiceCodec;
class UnixPipeNotifier;


class GattAudioPipe final : public QObject
{
	Q_OBJECT

public:
	enum InputCodec {
		IMADVICodec,
		G726Codec
	};
#if QT_VERSION > QT_VERSION_CHECK(5, 4, 0)
	Q_ENUM(InputCodec)
#else
	Q_ENUMS(InputCodec)
#endif

	enum OutputEncoding {
		PCM16,
		ADPCM
	};
#if QT_VERSION > QT_VERSION_CHECK(5, 4, 0)
	Q_ENUM(OutputEncoding)
#else
	Q_ENUMS(OutputEncoding)
#endif

public:
	explicit GattAudioPipe(OutputEncoding encoding, int outputPipeFd = -1,
	                       QObject *parent = nullptr);
	~GattAudioPipe() final;

public:
	bool isValid() const;

	bool isOutputOpen() const;

	bool start();
	void stop();

	int framesReceived() const;
	int framesExpected() const;

	FileDescriptor takeOutputReadFd();

	void addNotification(const quint8 value[20]);

signals:
	void outputPipeClosed();

private:
	void onOutputPipeException(int pipeFd);

	void processAudioFrame(const quint8 frame[100]);

private:
	const OutputEncoding m_encoding;

	VoiceCodec *m_codec;

	int m_outputPipeRdFd;
	int m_outputPipeWrFd;

	QSharedPointer<UnixPipeNotifier> m_outputPipeNotifier;

	quint8 m_frameBuffer[100];
	size_t m_frameBufferOffset;

	qint16 m_decodeBuffer[(96 * 2)];

	bool m_running;

	int m_frameCount;
	QElapsedTimer m_recordingTimer;
	qint64 m_recordingDuration;

	int m_missedSequences;
	quint8 m_lastSequenceNumber;
};


#endif // !defined(GATT_AUDIOPIPE_H)

