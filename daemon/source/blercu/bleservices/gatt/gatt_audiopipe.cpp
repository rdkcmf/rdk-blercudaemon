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
//  gatt_audiopipe.cpp
//  SkyBluetoothRcu
//

#include "gatt_audiopipe.h"

#include "utils/unixpipenotifier.h"
#include "utils/adpcmcodec.h"
#include "utils/logging.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>


#if defined(__APPLE__)
#  define pipe2(a, b)  pipe(a)
#endif






// -----------------------------------------------------------------------------
/*!
	\class GattAudioPipe
	\brief Reads data from GATT / bluez notification pipe, decodes the audio
	data and writes it to the output pipe.

	The source of the data is a notification pipe from bluez, over this we will
	get 20 byte packets that correspond to a notification from the RCU.  This
	class converts those to frames and then decodes them to PCM16 before writing
	them to the output pipe.

	When either the input or the output pipe is closed a corresponding signal
	is generated.

	When this object is destroyed both the input and output pipes are closed.

 */


// based on the data here http://soundfile.sapp.org/doc/WaveFormat/
struct __attribute__((packed)) WaveFileHeader {
	quint32 chunkId;            // 'RIFF'
	quint32 chunkSize;
	quint32 format;             // 'WAVE'

	quint32 fmtChunkId;         // 'fmt '
	quint32 fmtChunkSize;
	quint16 audioFormat;
	quint16 numberChannels;
	quint32 sampleRate;
	quint32 byteRate;
	quint16 blockAlign;
	quint16 bitsPerSample;

	quint32 dataChunkId;        // 'data'
	quint32 dataChunkSize;
};

#define RIFF   quint32(0x52494646)   // 'RIFF'
#define WAVE   quint32(0x57415645)   // 'WAVE'
#define FMT_   quint32(0x666d7420)   // 'fmt '
#define DATA   quint32(0x64617461)   // 'data'




// -----------------------------------------------------------------------------
/*!
	Constructs a new \l{GattAudioPipe} object without an input notifiction pipe.
	Use this constructor if you're manually injecting BLE GATT notifications
	into the pipe.

 */
GattAudioPipe::GattAudioPipe(int outputPipeFd, QObject *parent)
	: QObject(parent)
	, m_encoding(BleRcuAudioService::PCM16)
	, m_codec(new ADPCMCodec)
	, m_outputPipeRdFd(-1)
	, m_outputPipeWrFd(-1)
	, m_frameBufferOffset(0)
	, m_running(false)
	, m_frameCount(0)
	, m_recordingDuration(0)
	, m_missedSequences(0)
{

	if (outputPipeFd >= 0) {
		// dup the output file descriptor and use that as write end
		m_outputPipeWrFd = fcntl(outputPipeFd, F_DUPFD_CLOEXEC, 3);
		if (m_outputPipeWrFd < 0) {
			qErrnoWarning(errno, "failed to dup ouput file/fifo/pipe");
			return;
		}

		// set the pipe as non-blocking
		if (fcntl(m_outputPipeWrFd, F_SETFL, O_NONBLOCK) != 0)
			qErrnoWarning(errno, "failed to set O_NONBLOCK flag on pipe");


	} else {
		// create the new pipe for output
		int fds[2];
		if (pipe2(fds, O_CLOEXEC | O_NONBLOCK) != 0) {
			qErrnoWarning(errno, "failed to create output audio pipe");
			return;
		}

		m_outputPipeRdFd = fds[0];
		m_outputPipeWrFd = fds[1];
	}

	// final stage is to create listeners for the output pipe
	m_outputPipeNotifier =
		QSharedPointer<UnixPipeNotifier>(new UnixPipeNotifier(m_outputPipeWrFd),
		                                 &QObject::deleteLater);
	QObject::connect(m_outputPipeNotifier.data(), &UnixPipeNotifier::exceptionActivated,
	                 this, &GattAudioPipe::onOutputPipeException);

	// enable exception (pipe closed) events on the output pipe
	m_outputPipeNotifier->setExceptionEnabled(true);
}

// -----------------------------------------------------------------------------
/*!
	Destructs the object which involves just terminating all the event handlers
	and closing all the pipes.

 */
GattAudioPipe::~GattAudioPipe()
{
	// destroy the notifiers
	m_outputPipeNotifier.reset();

	// close any fds that we may still have open
	if ((m_outputPipeRdFd >= 0) && (::close(m_outputPipeRdFd) != 0))
		qErrnoWarning(errno, "failed to close output read pipe fd");

	if ((m_outputPipeWrFd >= 0) && (::close(m_outputPipeWrFd) != 0))
		qErrnoWarning(errno, "failed to close output write pipe fd");

	// destroy the codec
	if (m_codec) {
		delete m_codec;
		m_codec = nullptr;
	}
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if both the input and output pipes are currently open.

 */
bool GattAudioPipe::isValid() const
{
	return (m_outputPipeWrFd >= 0);
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if output pipe is not blocked, i.e. the remote end has not
	closed it's pipe.

 */
bool GattAudioPipe::isOutputOpen() const
{
	if (m_outputPipeWrFd < 0)
		return false;

	// try and empty write to the pipe, this will fail (with EPIPE) if the
	// read size is closed - even though we aren't sending anything
	return (::write(m_outputPipeWrFd, nullptr, 0) == 0);
}

// -----------------------------------------------------------------------------
/*!
	Sets the encoding of the data to be transmitted

 */
void GattAudioPipe::setEncoding(BleRcuAudioService::Encoding encoding)
{
	m_encoding = encoding;
}

// -----------------------------------------------------------------------------
/*!
	Starts the recording and streaming of data to the output pipe.

 */
bool GattAudioPipe::start()
{
	if (!isValid())
		return false;

	if (m_running) {
		qWarning("audio pipe already running");
		return false;
	}

	m_recordingTimer.start();
	m_frameCount = 0;
	m_recordingDuration = 0;
	m_missedSequences = 0;

	m_running = true;

	return true;
}

// -----------------------------------------------------------------------------
/*!
	Stops streaming data to the output pipe.

 */
void GattAudioPipe::stop()
{
	if (!m_running) {
		qWarning("audio pipe wasn't running");
		return;
	}

	m_running = false;

	m_recordingDuration = m_recordingTimer.elapsed();
	m_recordingTimer.invalidate();
}

// -----------------------------------------------------------------------------
/*!
	Simply returns the number of frames received.

 */
int GattAudioPipe::framesReceived() const
{
	return m_frameCount;
}

// -----------------------------------------------------------------------------
/*!
	Returns the number of frames expected.

	There are two ways of calculating this, one is to use the time length of
	the recording and then work out how many frames we should have received.
	The other is to use the sequence numbers to calculate how many frames we've
	missed.  Both have problems; the time base is just an estimate, whereas the
	sequence numbers can wrap (if you dropped more than 255 frames).  Also the
	sequence number wouldn't take in account packets dropped at the start of
	the recording.

	So the solution I came up with is to use the sequence numbers if they are
	within the ballpark of the time based estimate, and by ballpark I mean
	a 16 frame difference, which corresponds to 192ms (16 * 12ms per frame).

 */
int GattAudioPipe::framesExpected() const
{
	// calculate the time estimate
	const qint64 msecsElapsed = (m_recordingTimer.isValid()) ?
		m_recordingTimer.elapsed() : m_recordingDuration;

	int timeEstimate = static_cast<int>(msecsElapsed / 12);

	qDebug("audio frames expected: timeBased=%d, seqNumberBased=%d",
	       timeEstimate, (m_frameCount + m_missedSequences));

	// if the missed sequence count is within 16 frames of the time estimate
	// then use that, otherwise use the time
	int diff = timeEstimate - (m_frameCount + m_missedSequences);
	if (abs(diff) <= 16)
		return (m_frameCount + m_missedSequences);
	else
		return timeEstimate;
}

// -----------------------------------------------------------------------------
/*!
	Takes the read end of the output pipe, this is typically then passed on
	to AS to read the decoded audio from.

 */
FileDescriptor GattAudioPipe::takeOutputReadFd()
{
	if (Q_UNLIKELY(m_outputPipeRdFd < 0))
		return FileDescriptor();

	// the following will dup and store the fd
	FileDescriptor fd(m_outputPipeRdFd);

	// now close our internal copy
	if (::close(m_outputPipeRdFd) != 0)
		qErrnoWarning(errno, "failed to close read end of output pipe");
	m_outputPipeRdFd = -1;

	return fd;
}

// -----------------------------------------------------------------------------
/*!
	Call to manually inject a 20 byte notification into the pipe.  Only use this
 	if the object wasn't created with a notification pipe.

 */
void GattAudioPipe::addNotification(const quint8 value[20])
{
	// add the notification to the buffer, if we have a complete frame
	// then pass on to the decoder
	memcpy(m_frameBuffer + m_frameBufferOffset, value, 20);
	m_frameBufferOffset += 20;

	if (m_frameBufferOffset == 100) {
		m_frameBufferOffset = 0;

		if (Q_UNLIKELY(!m_running))
			qWarning("received GATT notification before pipe was running");
		else
			processAudioFrame(m_frameBuffer);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Decodes the audio frame and then writes the PCM 16-bit samples into the
	output pipe.

 */
void GattAudioPipe::processAudioFrame(const quint8 frame[100])
{
	const quint8 sequenceNumber = frame[0];
	const quint8 stepIndex = frame[1];
	const qint16 prevValue = (qint16(frame[2]) << 0) | (qint16(frame[3]) << 8);

	qDebug("frame: [%3hhu] <%3hhu,0x%04x> %02x %02x %02x ...",
	        sequenceNumber, stepIndex, quint16(prevValue), frame[4], frame[5], frame[6]);


	// if not running then just discard the frame
	if (!m_running)
		return;


	if (Q_LIKELY(m_frameCount != 0)) {

		// not the first frame so check the sequence number, this gives us how
		// many frames where lost, however the catch is the sequence number is
		// only 8-bits so it could wrap all the way around if we dropped 255
		// frames (~ 3 seconds of data)
		const quint8 expectedSeqNumber = m_lastSequenceNumber + 1;
		if (expectedSeqNumber != sequenceNumber) {
			quint8 missed = sequenceNumber - expectedSeqNumber;
			m_missedSequences += missed;
		}

	}

	// store the last sequence number
	m_lastSequenceNumber = sequenceNumber;

	// increment the count of audio frames received
	m_frameCount++;


	// feed the s/w decoder
	int frame_size = 100;
	if (m_encoding == BleRcuAudioService::PCM16) {
		m_codec->decodeFrame(stepIndex, prevValue, frame + 4, (96 * 2), m_decodeBuffer);
		frame_size = sizeof(m_decodeBuffer);
	}


	// write the pcm data into the output pipe
	if (Q_LIKELY(m_outputPipeWrFd >= 0)) {
		ssize_t wr;
		if (m_encoding == BleRcuAudioService::PCM16) {
			wr = TEMP_FAILURE_RETRY(::write(m_outputPipeWrFd, m_decodeBuffer, frame_size));
		} else {
			wr = TEMP_FAILURE_RETRY(::write(m_outputPipeWrFd, frame, frame_size));
		}
		if (wr < 0) {

			// check if the pipe is full
			if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
				qWarning("voice audio pipe to AS is full, frame discarded");

			} else {
				// check if AS closed the pipe, this is not an error so don't
				// log it as such
				if (errno == EPIPE)
					qInfo("output voice audio pipe closed by client");
				else
					qErrnoWarning(errno, "output voice audio pipe write failed");

				// close down the pipe
				onOutputPipeException(m_outputPipeWrFd);
			}

		} else if (wr != frame_size) {

			qWarning("only %zd of the possible %zu bytes of audio data could be"
			         " sent to AS", wr, frame_size);
		}

	}

}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when the output pipe is closed.  AS does this when it wants
	to stop audio streaming, so it is not handled as an error.

 */
void GattAudioPipe::onOutputPipeException(int pipeFd)
{
	if (Q_UNLIKELY(pipeFd != m_outputPipeWrFd))
		return;

	qDebug("detected close on the client output pipe");

	// stop receiving events and free the notifier
	m_outputPipeNotifier->setExceptionEnabled(false);
	m_outputPipeNotifier.reset();

	// close the output pipe
	if ((m_outputPipeWrFd >= 0) && (::close(m_outputPipeWrFd) != 0))
		qErrnoWarning(errno, "failed to close output pipe");
	m_outputPipeWrFd = -1;

	// let the parent statemachine know that the output pipe is closed (this
	// triggers the state-machine to ask the RCU to stop sending data)
	emit outputPipeClosed();
}


