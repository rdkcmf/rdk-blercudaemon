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
	IrSignalSet();
	explicit IrSignalSet(int codeId);
	IrSignalSet(const IrSignalSet &irSignalSet);
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
	{	return m_hash.keys();    }

	void clear();

	inline bool contains(const Qt::Key &key) const
	{	return m_hash.contains(key);  }

	QByteArray &operator[](const Qt::Key &key);
	const QByteArray operator[](const Qt::Key &key) const;

public:
	void insert(Qt::Key key, const QByteArray &data);
	void insert(Qt::Key key, QByteArray &&data);

};

#endif // !defined(IRSIGNALSET_H)

