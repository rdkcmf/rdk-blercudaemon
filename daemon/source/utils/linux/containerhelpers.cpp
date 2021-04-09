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
//  containerhelpers.cpp
//  BleRcuDaemon
//

#include "containerhelpers.h"
#include "containerhelpers_p.h"

#include <QFile>
#include <QDebug>
#include <QAtomicInteger>

#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <sys/syscall.h>


#if !defined(SYS_socketat)
#    if defined(__aarch64__) || defined(__AARCH64EB__) || defined(__AARCH64EL__)
#        define SYS_socketat   274
#    elif defined(__i686__)
#        define SYS_socketat   353
#    elif !(defined(__arm__) || defined(__ARMEB__) || defined(__ARMEL__))
#        error "Platforms does not support socketat call"
#    endif
#endif


#if 0
// -----------------------------------------------------------------------------
/*!
	\internal

	\warning don't use, this method is not fully implemented. Left here as a
	reference for if we ever move on to a newer kernel.

 */
static pid_t readProcStatNsPid()
{
	QFile procStatusFile("/proc/self/status");

	if (!procStatusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
		qWarning() << "failed to open" << procStatusFile.fileName();
		return -1;
	}

	QTextStream in(&procStatusFile);

	QString line = in.readLine();
	while (!line.isNull()) {

		if (line.startsWith("NSpid:")) {

			// TODO: implement

			break;
		}

		line = in.readLine();
	}

	return -1;
}
#endif

// -----------------------------------------------------------------------------
/*!
	\internal

	Reads the /proc/self/sched file and parses the first line to get the real
	pid of the process.  If you're outside a container this will be the same
	as the value returned by \c getpid(), if within a container it should be
	the pid outside the container.

 */
static pid_t readProcSched()
{
	// open the sched file relating to the current process
	QFile procSchedFile("/proc/self/sched");
	if (!procSchedFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
		qWarning() << "failed to open" << procSchedFile.fileName();
		return -1;
	}

	// read the first line only, don't care about the rest
	char lineBuf[128];
	if (procSchedFile.readLine(lineBuf, sizeof(lineBuf)) < 10) {
		qWarning() << "failed to read first line of" << procSchedFile.fileName();
		return -1;
	}

	// no longer need the proc file
	procSchedFile.close();

	// parse the string
	char comm[24];
	int realPid;
	int threadCounter;

	if (sscanf(lineBuf, "%16s (%d, #threads: %d)",
			   comm, &realPid, &threadCounter) != 3) {
		qWarning("failed to scan string '%s'", lineBuf);
		return -1;
	}

	// sanity check the pid
	if (realPid <= 0) {
		qWarning("invalid pid read from sched string '%s'", lineBuf);
		return -1;
	}

	qWarning("real pid of process is %d", realPid);
	return realPid;
}

// -----------------------------------------------------------------------------
/*!
	Gets the real pid of the current process as seen from outside a container.

	The value is only read once and then cached for all subsequent calls.
 */
pid_t getRealProcessId()
{
	static QAtomicInteger<pid_t> realPid(-1);

	if (realPid > 0)
		return realPid;

	// there are two ways of getting th real pid, a nice way and a hacky way:
	//
	//   nice) since linux 4.1 a new field was added to /proc/<pid>/status
	//         called nsPid, this lists the pid as shown in all the pid
	//         namespaces that the process belongs to. However we're
	//         running on a 3.10.27 kernel so ...
	//
	//   hack) the /proc/<pid>/sched file still lists the pid outside all
	//         pid namespaces, so we can simply use that to get the real pid.
	//         It's a hack because we're exploiting what is in effect a bug in
	//         the kernel.
	//

//	pid_t realPid = readProcStatNsPid();
//	if (realPid > 0)
//		return realPid;

	pid_t pid = readProcSched();
	if (pid > 0)
		realPid.store(pid);

	return realPid;
}

// -----------------------------------------------------------------------------
/*!
	A syscall that creates a socket in a specific network namespace

	This syscall has been specifically added to Sky/Ethan kernels, it's based
	off the following kernel patch:
		https://lwn.net/Articles/407613/

	The socketat syscall does the equivalent of the following
		setns(newNetworkNsFd)
		socket(...)
		setns(origNetworkNsFd)

	However the downside with the above is that you currently need to be root
	user (CAP_SYS_ADMIN) to call setns, and it's generally more faff.

	socketat takes the same parameters as the usual socket call, however it takes
	an additional file descriptor which is an open handle to the desired network
	namespace to create the socket in.

 */
int createSocketInNs(int netNsFd, int domain, int type, int protocol)
{
#if !defined(SYS_socketat)
	// we must be on an Arm (non 64-bit) platform and the therefore the syscall
	// numbers depend on the version of the kernel
	static long SYS_socketat = -1;
	if (SYS_socketat < 0) {
		struct utsname buf;
		if (uname(&buf) < 0)
			return -1;

		// the following are the syscall numbers based on current kernel
		// versions:
		//    3.10.27 -  380    (falcon-d1, mr, xwing)
		//    3.10.92 -  380    (falcon-v2, amidala)
		//    4.9.51  -  397    (titan)
		if (strverscmp(buf.release, "3.11") < 0)
			__sync_bool_compare_and_swap(&SYS_socketat, -1, 380);
		else
			__sync_bool_compare_and_swap(&SYS_socketat, -1, 397);
	}
#endif

    return syscall(SYS_socketat, netNsFd, domain, type, protocol);
}



NetworkNamespaceThread::NetworkNamespaceThread(int netNsFd,
                                               const std::function<void()> &function)
	: m_netNsFd(netNsFd)
	, m_function(function)
	, m_success(false)
{
}

bool NetworkNamespaceThread::success() const
{
	return m_success;
}

// -----------------------------------------------------------------------------
/*!
	\internal
 
	Thread runner that just attempts to switch into the supplied network
	namespace and then execute the caller supplied function.

 */
void NetworkNamespaceThread::run()
{
	m_success = false;

#if !defined(__linux__)
	qWarning("this method only works on linux");

#else
	// try and switch into the new network namespace
	if (setns(m_netNsFd, CLONE_NEWNET) != 0) {
		qErrnoWarning(errno, "failed to switch into new namespace");
		m_success = false;
		return;
	}

	// now execute the supplied function
	if (m_function)
		m_function();

	// and we're done
	m_success = true;
#endif

	return;
}

// -----------------------------------------------------------------------------
/*!
	Runs the supplied \a function in the network namespace given by the
	\a netNsFd file descriptor.

	If the function was run \c true is returned, otherwise \c false.
 
	This will spawn a new thread - which is a requirement for switching
	namespaces - and execute the function there, however it will block until
	the thread completes.

	\warning This function will only work if the user namespace that the
	network namespace was created in matches the current user namespace. See
	[https://github.com/opencontainers/runc/issues/771] for more info. In
	reality what this means is that you can't have userns enabled and also use
	this function.

 */
bool runInNetworkNamespaceImpl(int netNsFd, const std::function<void()> &function)
{
	NetworkNamespaceThread setNetNs(netNsFd, function);

	setNetNs.start();
	setNetNs.wait();

	return setNetNs.success();
}


