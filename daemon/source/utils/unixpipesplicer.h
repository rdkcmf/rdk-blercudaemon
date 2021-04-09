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
//  unixpipesplicer.h
//  BleRcuDaemon
//

#ifndef UNIXPIPESPLICER_H
#define UNIXPIPESPLICER_H

#include <QObject>
#include <QSharedPointer>
#include <QSocketNotifier>
#include <QAtomicInteger>

#include <sys/types.h>

class UnixPipeNotifier;

class UnixPipeSplicer : public QObject
{
	Q_OBJECT

public:
	enum Mode {
		Block,
		FreeFlow,
	};

public:
	UnixPipeSplicer(int readFd, int writeFd, Mode mode = Block);
	~UnixPipeSplicer();

public:
	quint64 bytesRx() const;
	quint64 bytesTx() const;

public slots:
	void start();
	void stop();

	// TODO: void closeReadSide();
	void closeWriteSide();

signals:
	void started();
	void stopped();

	void writeException();
	void readException();

private slots:
	void onReadActivated(int fd);
	void onWriteActivated(int fd);
	void onWriteException(int fd);

private:
	void doStart();
	void doStop();

	void onOutputClosed();
#if !defined(__linux__)
	static const int SPLICE_F_MOVE = 0x01;
	static const int SPLICE_F_NONBLOCK = 0x02;
	ssize_t splice(int fd_in, off_t *off_in, int fd_out,
	               off_t *off_out, size_t len, unsigned int flags) const;
#endif // !defined(__linux__)

private:
	static long m_pageSize;
	
private:
	const Mode m_mode;

	int m_readFd;
	int m_writeFd;

	bool m_readException;
	bool m_writeException;

	QSharedPointer<QSocketNotifier> m_readNotifier;
	QSharedPointer<UnixPipeNotifier> m_writeNotifier;

	bool m_inThrowAwayMode;
	QAtomicInteger<quint64> m_bytesRx;
	QAtomicInteger<quint64> m_bytesTx;

};

#endif // !defined(UNIXPIPESPLICER_H)
