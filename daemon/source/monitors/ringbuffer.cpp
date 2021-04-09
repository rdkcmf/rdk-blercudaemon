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
//  ringbuffer.cpp
//  BleRcuDaemon
//

#include "ringbuffer.h"

#include <QDebug>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>



// -----------------------------------------------------------------------------
/*!
	\class RingBuffer
	\brief Object that provides a low level ring buffer.

	This class provides a low level implementation of a ring buffer that uses
	some virtual memory tricks to provide a continous memory range to the
	clients.

	

 */


// -----------------------------------------------------------------------------
/*!
	\internal

	Small utility to sanitise the size of the buffer so that it is at least
	the mimimum size and it's a multiple of a page.
 */
static inline size_t sanitiseBufferSize(size_t size)
{
	static const size_t pageSize = sysconf(_SC_PAGE_SIZE);

	size = qMax<size_t>(size, pageSize);
	return (size + (pageSize - 1)) & ~(pageSize - 1);
}


// -----------------------------------------------------------------------------
/*!
	Constructs an invalid ring buffer.

 */
RingBuffer::RingBuffer()
	: m_buffer(nullptr)
	, m_size(0)
	, m_headIndex(0)
	, m_tailIndex(0)
{
}

// -----------------------------------------------------------------------------
/*!
	Constructs a ring buffer of at least \a size bytes, the actual size of the
	buffer may be increased to meet minium size and page alignment restrictions.
 
	Use isValid() to determine if the ring buffer was successifully created.

 */
RingBuffer::RingBuffer(size_t unalignedSize)
	: m_buffer(nullptr)
	, m_size(sanitiseBufferSize(unalignedSize))
	, m_headIndex(0)
	, m_tailIndex(0)
{
	char shmName[32];
	sprintf(shmName, "/buffer-%08x", qrand());

	// create a shared memory block of the correct buffer size
	int shmFd = shm_open(shmName, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (shmFd < 0) {
		qErrnoWarning(errno, "failed to create shm for buffer");
		return;
	}

	if (shm_unlink(shmName) != 0)
		qErrnoWarning(errno, "failed to unlink shm");

	if (ftruncate(shmFd, m_size) != 0) {
		qErrnoWarning(errno, "failed to resize shm for buffer");
		close(shmFd);
		return;
	}


	// now the tricky part, we need to reserve some virtual memory that is
	// twice as large as the buffer
	void *reserveMap = mmap(nullptr, (m_size * 2), PROT_NONE,
	                        (MAP_PRIVATE | MAP_ANONYMOUS), -1, 0);
	if (reserveMap == MAP_FAILED) {
		qErrnoWarning(errno, "failed to reserve virtual space for the buffer");
		close(shmFd);
		return;
	}

	// now overlap the shm buffer at the start of the reserve mapping created
	// above
	void *bufMap0 = mmap(reinterpret_cast<void*>(uintptr_t(reserveMap) + 0),
	                     m_size, (PROT_READ | PROT_WRITE),
	                     (MAP_SHARED | MAP_FIXED), shmFd, 0);
	if (bufMap0 == MAP_FAILED) {
		qErrnoWarning(errno, "failed to overlap shm buffer 0");
		munmap(reserveMap, (m_size * 2));
		close(shmFd);
		return;
	}

	// and map again right after the first mapping
	void *bufMap1 = mmap(reinterpret_cast<void*>(uintptr_t(reserveMap) + m_size),
	                     m_size, (PROT_READ | PROT_WRITE),
	                     (MAP_SHARED | MAP_FIXED), shmFd, 0);
	if (bufMap1 == MAP_FAILED) {
		qErrnoWarning(errno, "failed to overlap shm buffer 1");
		munmap(reserveMap, (m_size * 2));
		close(shmFd);
		return;
	}

	// it is safe to close the fd now we've mapped it
	if (close(shmFd) != 0)
		qErrnoWarning(errno, "failed to close shm");


	qDebug("mapped ring buffer to 0x%p and 0x%p with size 0x%08zx",
	       bufMap0, bufMap1, m_size);

	m_buffer = reinterpret_cast<quint8*>(bufMap0);
}

// -----------------------------------------------------------------------------
/*!
	Destructor that cleans up the ring buffer.

 */
RingBuffer::~RingBuffer()
{
	// we unmap twice the size of the buffer as we mmap it twice
	if (m_buffer != nullptr)
		munmap(m_buffer, (m_size * 2));

	m_buffer = nullptr;
}
