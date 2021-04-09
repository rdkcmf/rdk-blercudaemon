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
//  irsignalset.cpp
//  SkyBluetoothRcu
//

#include "irsignalset.h"



IrSignalSet::IrSignalSet()
	: m_codeId(-1)
{
}

IrSignalSet::IrSignalSet(int codeId)
	: m_codeId(codeId)
{
}

IrSignalSet::IrSignalSet(const IrSignalSet &irSignalSet)
	: m_codeId(irSignalSet.m_codeId)
	, m_hash(irSignalSet.m_hash)
{
}

void IrSignalSet::clear()
{
	m_hash.clear();
}

QByteArray &IrSignalSet::operator[](const Qt::Key &key)
{
	return m_hash[key];
}

const QByteArray IrSignalSet::operator[](const Qt::Key &key) const
{
	return m_hash.value(key);
}

void IrSignalSet::insert(Qt::Key key, const QByteArray &data)
{
	m_hash.insert(key, data);
}

void IrSignalSet::insert(Qt::Key key, QByteArray &&data)
{
	m_hash.insert(key, std::move(data));
}
