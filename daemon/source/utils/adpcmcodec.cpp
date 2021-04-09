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
//  adpcmcodec.cpp
//  SkyBluetoothRcu
//

#include "adpcmcodec.h"

#include <QtEndian>


const int ADPCMCodec::m_indexTable[16] = {
	-1, -1, -1, -1, 2, 4, 6, 8,
	-1, -1, -1, -1, 2, 4, 6, 8
};

const int ADPCMCodec::m_stepSizeTable[89] = {
	    7,     8,     9,    10,    11,    12,    13,    14,    16,    17,
	   19,    21,    23,    25,    28,    31,    34,    37,    41,    45,
	   50,    55,    60,    66,    73,    80,    88,    97,   107,   118,
	  130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
	  337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
	  876,   963,  1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
	 2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
	 5894,  6484,  7132,  7845,  8630,  9493, 10442, 11487, 12635, 13899,
	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};


ADPCMCodec::ADPCMCodec()
{
	resetStream(0, 0);
}

ADPCMCodec::~ADPCMCodec()
{
}

#if 0
// -----------------------------------------------------------------------------
/*!
	\internal

	Decodes a single ADPCM sample, encodeded into the lower 4 bits or \a sample,
	into a signed 16-bit PCM value.

	\note This method is currently not used, left here for reference.
 */
qint16 ADPCMCodec::decodeSample(quint8 sample)
{
	int step = m_stepSizeTable[m_stepIndex];

	m_stepIndex += m_indexTable[(sample & 0xF)];
	m_stepIndex = qBound<int>(0, m_stepIndex, 88);

	int diff = (step >> 3);
	if (sample & 0x1)
		diff += (step >> 2);
	if (sample & 0x2)
		diff += (step >> 1);
	if (sample & 0x4)
		diff += (step >> 0);

	if (sample & 0x8)
		m_previousValue -= diff;
	else
		m_previousValue += diff;

	m_previousValue = qBound<int>(-32768, m_previousValue, 32767);
	return m_previousValue;
}
#endif

// -----------------------------------------------------------------------------
/*!
	\internal

	Performs the ADCPM decode.  Note that the \a stepIndex and \a prevValue
	are reference parameters so are updated as the audio data is decoded.

	This code based on [http://yxit.co.uk/source/documentation/IMA__ADPCM_8cpp_source.html]

 */
void ADPCMCodec::decodeFrameAndUpdate(int &stepIndex, qint16 &prevValue,
                                      const quint8 *samples, int numSamples,
                                      qint16 *pcmSamples) const
{
	// code based on
	// http://yxit.co.uk/source/documentation/IMA__ADPCM_8cpp_source.html

	// sanity checks
	if (Q_UNLIKELY(!samples || !pcmSamples || (numSamples <= 0)))
		return;

	for (int i = 0; i < numSamples; i++) {

		// push the sample into the lower 4 bits of sample, we always do the
		// upper nibble then the lower nibble
		quint8 sample = samples[(i / 2)];
		if ((i & 0x1) == 0)
			sample >>= 4;

		// do the decode
		int step = m_stepSizeTable[stepIndex];

		stepIndex += m_indexTable[(sample & 0xF)];
		stepIndex = qBound<int>(0, stepIndex, 88);

		int diff = (step >> 3);
		if (sample & 0x1)
			diff += (step >> 2);
		if (sample & 0x2)
			diff += (step >> 1);
		if (sample & 0x4)
			diff += (step >> 0);

		if (sample & 0x8)
			prevValue -= diff;
		else
			prevValue += diff;

		prevValue = qBound<int>(-32768, prevValue, 32767);

		// write the output value if space in the buffer
		*pcmSamples++ = qToLittleEndian<qint16>(prevValue);
	}

}

// -----------------------------------------------------------------------------
/*!
	Decodes the ADPCM stored in \a samples with length \a numSamples to the
	\a pcmSamples buffer.  The samples are converted into signed 16-bit PCM in
	little endian bytes order.

	\a numSamples should be the number of samples in \a samples, each sample is
	4-bits so \a numSamples will be twice the number of bytes supplied.

	\a pcmSamples should point to a buffer that can store at least the
	\a numSamples of samples.

 */
void ADPCMCodec::decodeFrame(int stepIndex, qint16 prevValue,
                             const quint8 *samples, int numSamples,
                             qint16 *pcmSamples) const
{
	decodeFrameAndUpdate(stepIndex, prevValue, samples, numSamples, pcmSamples);
}

// -----------------------------------------------------------------------------
/*!
	Resets the decoder state to initial values.  This should be used when there
	is an interruption in the stream of ADPCM samples.

 */
void ADPCMCodec::resetStream(int stepIndex, qint16 prevValue)
{
	m_streamStepIndex = stepIndex;
	m_streamPrevValue = prevValue;
}

// -----------------------------------------------------------------------------
/*!
	Decodes the ADPCM stored in \a samples with length \a numSamples to the
	\a pcmSamples buffer.  The samples are converted into signed 16-bit PCM in
	little endian bytes order. This function uses the internal stream step
	index and the last decoded value to perform the decode.

	\a numSamples should be the number of samples in \a samples, each sample is
	4-bits so \a numSamples will be twice the number of bytes supplied.

	\a pcmSamples should point to a buffer that can store at least the
	\a numSamples of samples.

 */
void ADPCMCodec::decodeStream(const quint8 *samples, int numSamples,
                              qint16 *pcmSamples)
{
	decodeFrameAndUpdate(m_streamStepIndex, m_streamPrevValue,
	                     samples, numSamples, pcmSamples);
}

