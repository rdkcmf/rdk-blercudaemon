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
//  inputdeviceinfo.cpp
//  SkyBluetoothRcu
//

#include "inputdeviceinfo.h"

#include "linux/linuxinputdeviceinfo.h"



InputDeviceInfo::InputDeviceInfo()
{
}

InputDeviceInfo::InputDeviceInfo(const InputDeviceInfo &other)
	: _d(other._d)
{
}

InputDeviceInfo::~InputDeviceInfo()
{
}

InputDeviceInfo & InputDeviceInfo::operator=(const InputDeviceInfo &other)
{
	if (&other != this)
		_d = other._d;

	return *this;
}

bool InputDeviceInfo::operator==(const InputDeviceInfo &other) const
{
	if (!_d || !other._d)
		return false;

#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
	return _d->isEqual(other._d.data());
#else
	return _d->isEqual(other._d.get());
#endif
}


InputDeviceInfo::InputDeviceInfo(const QSharedPointer<LinuxInputDeviceInfo> &deviceInfo)
	: _d(deviceInfo)
{
}


bool InputDeviceInfo::isNull() const
{
	return _d.isNull() || _d->isNull();
}

int InputDeviceInfo::id() const
{
	if (Q_UNLIKELY(_d.isNull()))
		return -1;

	return _d->id();
}

QString	InputDeviceInfo::name() const
{
	if (Q_UNLIKELY(_d.isNull()))
		return QString();

	return _d->name();
}

bool InputDeviceInfo::hasBusType() const
{
	if (Q_UNLIKELY(_d.isNull()))
		return false;

	return _d->hasBusType();
}

InputDeviceInfo::BusType InputDeviceInfo::busType() const
{
	if (Q_UNLIKELY(_d.isNull()))
		return InputDeviceInfo::Other;

	return _d->busType();
}

bool InputDeviceInfo::hasProductIdentifier() const
{
	if (Q_UNLIKELY(_d.isNull()))
		return false;

	return _d->hasProductIdentifier();
}

quint16	InputDeviceInfo::productIdentifier() const
{
	if (Q_UNLIKELY(_d.isNull()))
		return 0x0000;

	return _d->productIdentifier();
}

bool InputDeviceInfo::hasVendorIdentifier() const
{
	if (Q_UNLIKELY(_d.isNull()))
		return false;

	return _d->hasVendorIdentifier();
}

quint16	InputDeviceInfo::vendorIdentifier() const
{
	if (Q_UNLIKELY(_d.isNull()))
		return 0x0000;

	return _d->vendorIdentifier();
}

bool InputDeviceInfo::hasVersion() const
{
	if (Q_UNLIKELY(_d.isNull()))
		return false;

	return _d->hasVersion();
}

quint16	InputDeviceInfo::version() const
{
	if (Q_UNLIKELY(_d.isNull()))
		return 0x0000;

	return _d->version();
}

bool InputDeviceInfo::matches(const BleAddress &address) const
{
	if (Q_UNLIKELY(_d.isNull()))
		return false;

	return _d->matches(address);
}

QDebug operator<<(QDebug dbg, const InputDeviceInfo &info)
{
	dbg << "InputDeviceInfo(";
	if (info._d.isNull())
		dbg << "null";
	else
		dbg << *info._d;
	dbg << ")";

	return dbg;
}
