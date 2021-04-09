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
//  audiowavfile.cpp
//  SkyBluetoothRcu
//

#include "audiowavfile.h"

#include <QtEndian>

#include <QDebug>
#include <QSocketNotifier>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>



// based on the data here http://soundfile.sapp.org/doc/WaveFormat/
struct __attribute__((packed)) WaveFileHeader {
	quint32 chunkId;			// 'RIFF'
	quint32 chunkSize;
	quint32 format;				// 'WAVE'

	quint32 fmtChunkId;			// 'fmt '
	quint32 fmtChunkSize;
	quint16 audioFormat;
	quint16 numberChannels;
	quint32 sampleRate;
	quint32 byteRate;
	quint16 blockAlign;
	quint16 bitsPerSample;

	quint32 dataChunkId;		// 'data'
	quint32 dataChunkSize;
};

#define RIFF   quint32(0x52494646)   // 'RIFF'
#define WAVE   quint32(0x57415645)   // 'WAVE'
#define fmt_   quint32(0x666d7420)   // 'fmt '
#define data   quint32(0x64617461)   // 'data'






AudioWavFile::AudioWavFile(const QString &filePath)
	: m_file(filePath)
	, m_pipeFd(-1)
	, m_pipeNotifier(nullptr)
{

	// try and open / create the output file
	if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qWarning() << "failed to open file" << filePath
		           << "due to" << m_file.error();
		return;
	}

	// write the wave file header, we obviously don't know the data size till
	// we're closing the file so leave that blank and we'll come back and add
	// that at the end
	m_dataWritten = 0;

	// write the WAVE header
	if (!writeFileHeader()) {
		qWarning("failed to write WAVE file header");
		m_file.close();
	}

}

AudioWavFile::~AudioWavFile()
{
	// destroy the notifier
	if (m_pipeNotifier) {
		m_pipeNotifier->setEnabled(false);
		delete m_pipeNotifier;
	}

	// close the pipe
	if ((m_pipeFd >= 0) && (close(m_pipeFd) != 0))
		qErrnoWarning(errno, "failed to close audio pipe");

	// if the output file was opened then go back and update the WAV header
	if (m_file.isOpen()) {

		if (!m_file.seek(0) || !writeFileHeader())
			qWarning("failed to update the WAV file header before closing");

		m_file.flush();
		m_file.close();
	}
}

// -----------------------------------------------------------------------------
/*!
	Returns true if the device is open; otherwise returns false. A device is
	open if it can be read from and/or written to.

 */
bool AudioWavFile::isOpen() const
{
	return m_file.isOpen();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Writes the WAV file header at the current file position in the file.

 */
bool AudioWavFile::writeFileHeader()
{
	// create and write the WAVE header
	WaveFileHeader header;
	bzero(&header, sizeof(WaveFileHeader));

	header.chunkId = qToBigEndian<quint32>(RIFF);
	header.chunkSize = static_cast<quint32>(4 + (8 + 16) + (8 + m_dataWritten));
	header.format = qToBigEndian<quint32>(WAVE);

	header.fmtChunkId = qToBigEndian<quint32>(fmt_);
	header.fmtChunkSize = 16;
	header.audioFormat = 0x0001;
	header.numberChannels = 1;
	header.sampleRate = 16000;
	header.byteRate = (16000 * 2);
	header.blockAlign = 2;
	header.bitsPerSample = 16;

	header.dataChunkId = qToBigEndian<quint32>(data);
	header.dataChunkSize = static_cast<quint32>(m_dataWritten);

	return m_file.write((const char*)&header, sizeof(header)) == sizeof(header);
}

// -----------------------------------------------------------------------------
/*!


 */
void AudioWavFile::setPipeSource(int pipeFd)
{
	// clean up the old pipe
	onPipeClosed();

	// dup the pipe fd
	m_pipeFd = fcntl(pipeFd, F_DUPFD_CLOEXEC, 3);
	if (m_pipeFd <= 0) {
		qErrnoWarning(errno, "failed to dup audio pipe");
		return;
	}

	if (fcntl(m_pipeFd, F_SETFL, O_NONBLOCK) != 0)
		qErrnoWarning(errno, "failed to to set the pipe to be non-blocking");

	// create a socket notifier and attach to the pipe
	m_pipeNotifier = new QSocketNotifier(m_pipeFd, QSocketNotifier::Read);
	QObject::connect(m_pipeNotifier, &QSocketNotifier::activated,
	                 this, &AudioWavFile::onPipeData,
	                 Qt::QueuedConnection);
}

// -----------------------------------------------------------------------------
/*!
 	\internal

 */
void AudioWavFile::onPipeData(int pipeFd)
{
	if (Q_UNLIKELY(pipeFd != m_pipeFd))
		return;

	quint8 buffer[512];
	while (true) {

		qint64 rd = TEMP_FAILURE_RETRY(::read(m_pipeFd, buffer, sizeof(buffer)));
		if (rd < 0) {

			// check if the pipe is empty
			if (errno == EAGAIN)
				break;

			// otherwise some other fatal error happened
			qErrnoWarning(errno, "failed to read from the audio pipe");
			onPipeClosed();
			break;

		} else if (rd == 0) {

			// the other side has closed the pipe, flush the file contents and
			// and update the header
			onPipeClosed();
			break;

		} else {

			// copy the pcm data into the file
			if (m_file.write((const char*)buffer, rd) != rd) {
				qWarning("failed to write audio sample data");
			}

			m_dataWritten += rd;

		}

	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
void AudioWavFile::onPipeClosed()
{
	// destroy the pipe notifier
	if (m_pipeNotifier) {
		m_pipeNotifier->setEnabled(false);
		delete m_pipeNotifier;
	}

	// close the pipe fd
	if ((m_pipeFd >= 0) && (close(m_pipeFd) != 0))
		qErrnoWarning(errno, "failed to close audio pipe");
	m_pipeFd = -1;

	// if the output file was opened then go back and update the WAV header
	if (m_file.isOpen()) {

		// get the current position and then re-write the header
		qint64 currentPos = m_file.pos();

		if (!m_file.seek(0) || !writeFileHeader())
			qWarning("failed to update the WAV file header before closing");

		m_file.flush();

		// return to the previous position
		m_file.seek(currentPos);
	}

}

