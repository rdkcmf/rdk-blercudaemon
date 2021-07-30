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
//  irsignalset.h
//  SkyBluetoothRcu
//

#ifndef IRSIGNALSET_H
#define IRSIGNALSET_H

#include <QDebug>
#include <QHash>
#include <QByteArray>


class IrSignalSet
{
public:
	IrSignalSet()
		: m_codeId(-1)
	{}

	explicit IrSignalSet(int codeId)
		: m_codeId(codeId)
	{}

	IrSignalSet(const IrSignalSet &irSignalSet)
		: m_codeId(irSignalSet.m_codeId)
		, m_hash(irSignalSet.m_hash)
    {}

	~IrSignalSet() = default;

private:
	const int m_codeId;
	QHash<Qt::Key, QByteArray> m_hash;

public:
	inline bool isValid() const
	{	return (m_codeId > 0); }

	inline int codeId() const
	{	return m_codeId; }

	inline int size() const
	{	return m_hash.size(); }

	inline int count() const
	{	return m_hash.size(); }

	inline bool isEmpty() const
	{	return m_hash.isEmpty(); }

	inline QList<Qt::Key> keys() const
	{	return m_hash.keys(); }

	void clear()
	{	m_hash.clear(); }

	inline bool contains(const Qt::Key &key) const
	{	return m_hash.contains(key);  }

	QByteArray &operator[](const Qt::Key &key)
	{	return m_hash[key]; }

	const QByteArray operator[](const Qt::Key &key) const
	{	return m_hash.value(key); }

public:
	void insert(Qt::Key key, const QByteArray &data)
	{	m_hash.insert(key, data); }

	void insert(Qt::Key key, QByteArray &&data)
	{	m_hash.insert(key, std::move(data)); }

};

#endif // !defined(IRSIGNALSET_H)

