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
//  ringbuffer.h
//  BleRcuDaemon
//

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <QtGlobal>


class RingBuffer
{
public:
	RingBuffer();
	RingBuffer(size_t size);
	~RingBuffer();

public:
	inline bool isValid() const
	{
		return (m_buffer != nullptr);
	}

	inline size_t space() const
	{
		return m_size - (m_headIndex - m_tailIndex) - 1;
	}
	inline size_t size() const
	{
		return (m_headIndex - m_tailIndex);
	}

	inline bool isEmpty() const
	{
		return (m_tailIndex == m_headIndex);
	}

	inline void clear()
	{
		m_tailIndex = m_headIndex = 0;
	}

	inline void advanceTail(size_t amount)
	{
		if (Q_UNLIKELY((m_headIndex - m_tailIndex) < amount))
			m_tailIndex = m_headIndex;
		else
			m_tailIndex += amount;

		// check if we've moved into the second map and if so reset both indexes
		// back into the first mapping
		if (Q_UNLIKELY(m_tailIndex >= m_size)) {
			m_tailIndex -= m_size;
			m_headIndex -= m_size;
		}
	}

	inline void advanceHead(size_t amount)
	{
		const size_t avail = space();
		if (Q_UNLIKELY(avail < amount))
			m_headIndex += avail;
		else
			m_headIndex += amount;
	}


	template<class T>
	inline T* data()
	{
		return reinterpret_cast<T*>(m_buffer);
	}
	template<class T>
	inline const T* data() const
	{
		return reinterpret_cast<const T*>(m_buffer);
	}


	template<class T>
	inline T* head()
	{
		return reinterpret_cast<T*>(m_buffer + m_headIndex);
	}
	template<class T>
	inline const T* head() const
	{
		return reinterpret_cast<const T*>(m_buffer + m_headIndex);
	}


	template<class T>
	inline T* tail()
	{
		return reinterpret_cast<T*>(m_buffer + m_tailIndex);
	}
	template<class T>
	inline const T* tail() const
	{
		return reinterpret_cast<const T*>(m_buffer + m_tailIndex);
	}

private:
	quint8 *m_buffer;

	const size_t m_size;
	size_t m_headIndex;
	size_t m_tailIndex;

private:
	Q_DISABLE_COPY(RingBuffer)
};


#endif // !defined(RINGBUFFER_H)
