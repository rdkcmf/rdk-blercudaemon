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

#ifndef DUMPER_H
#define DUMPER_H

#include <QStack>
#include <QDebug>
#include <QString>

#include <cstdarg>


class Dumper
{

private:
	struct Stream {

		explicit Stream(int fd_) : fd(fd_), indent(0), ref(1) {}

		int fd;
		int indent;
		QAtomicInt ref;
		QStack<int> indentStack;
		static const char indentBuf[64];

		void printLine(const char *format, va_list args);
		void printNewline();

	} *m_stream;

public:
	explicit inline Dumper(int fd) : m_stream(new Stream(fd)) {}
	inline Dumper(const Dumper &o) : m_stream(o.m_stream) { m_stream->ref.ref(); }
	inline Dumper &operator=(const Dumper &other);
	~Dumper();

	inline void swap(Dumper &other) Q_DECL_NOTHROW { qSwap(m_stream, other.m_stream); }

public:
	inline int indent() const
	{
		return m_stream->indent;
	}
	inline void pushIndent(int indent)
	{
		if ((m_stream->indent + indent) >= sizeof(Stream::indentBuf))
			indent = 0;

		m_stream->indentStack.push(indent);
		m_stream->indent += indent;
	}
	inline void popIndent()
	{
		if (!m_stream->indentStack.isEmpty())
			m_stream->indent -= m_stream->indentStack.pop();
	}

	inline void printLine(const char *format, ...) const __attribute__((format(printf, (2), (3))))
	{
		va_list args;
		va_start(args, format);
		m_stream->printLine(format, args);
		va_end(args);
	}

	inline void printNewline() const
	{
		m_stream->printNewline();
	}

	inline void printBoolean(const char *prefix, bool value) const
	{
		printLine("%s%s", prefix, value ? "true" : "false");
	}

	inline void printString(const char *prefix, const QString &string) const
	{
		printLine("%s%s", prefix, qPrintable(string));
	}

};

inline Dumper &Dumper::operator=(const Dumper &other)
{
	if (this != &other) {
		Dumper copy(other);
		qSwap(m_stream, copy.m_stream);
	}
	return *this;
}


#endif // !defined(DUMPER_H)
