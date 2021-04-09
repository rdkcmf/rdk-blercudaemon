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
//  adpcmcodec.h
//  SkyBluetoothRcu
//

#ifndef ADPCMCODEC_H
#define ADPCMCODEC_H

#include "voicecodec.h"


class ADPCMCodec : public VoiceCodec
{
public:
	ADPCMCodec();
	~ADPCMCodec();

public:
	void decodeFrame(int stepIndex, qint16 prevValue,
	                 const quint8 *samples, int numSamples,
	                 qint16 *pcmSamples) const override;

	void resetStream(int stepIndex = 0, qint16 prevValue = 0) override;
	void decodeStream(const quint8 *samples, int numSamples,
	                  qint16 *pcmSamples) override;

private:
	void decodeFrameAndUpdate(int &stepIndex, qint16 &prevValue,
	                          const quint8 *samples, int numSamples,
	                          qint16 *pcmSamples) const;

private:
	static const int m_indexTable[16];
	static const int m_stepSizeTable[89];

	int m_streamStepIndex;
	qint16 m_streamPrevValue;
};



#endif // !defined(ADPCMCODEC_H)
