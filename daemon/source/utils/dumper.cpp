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
//  dumper.h
//  SkyBluetoothRcu
//

#include "dumper.h"

#include <errno.h>
#include <unistd.h>
#include <sys/uio.h>


const char Dumper::Stream::indentBuf[64] = "                                                               ";

Dumper::~Dumper()
{
	if (!m_stream->ref.deref())
		delete m_stream;
}

void Dumper::Stream::printNewline()
{
	if (Q_UNLIKELY(fd < 0))
		return;

	struct iovec iov[2];

	iov[0].iov_base = const_cast<char*>(indentBuf);
	iov[0].iov_len = static_cast<size_t>(indent);

	iov[1].iov_base = const_cast<char*>("\n");
	iov[1].iov_len = 1;

	writev(fd, iov, 2);
}

void Dumper::Stream::printLine(const char *format, va_list args)
{
	if (Q_UNLIKELY(fd < 0))
		return;

	char lineBuf[512];
	int len = vsnprintf(lineBuf, sizeof(lineBuf), format, args);

	struct iovec iov[3];

	iov[0].iov_base = const_cast<char*>(indentBuf);
	iov[0].iov_len = static_cast<size_t>(indent);

	iov[1].iov_base = lineBuf;
	iov[1].iov_len = qMin<size_t>((sizeof(lineBuf) - 1), len);

	iov[2].iov_base = const_cast<char*>("\n");
	iov[2].iov_len = 1;

	writev(fd, iov, 3);
}
