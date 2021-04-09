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
//  linuxinputdevice.h
//  SkyBluetoothRcu
//

#ifndef LINUXINPUTDEVICE_H
#define LINUXINPUTDEVICE_H

#include "../inputdevice.h"

#include <QString>
#include <QSocketNotifier>

#include <sys/types.h>

struct input_event;
class LinuxInputDeviceInfo;

class LinuxInputDevice : public InputDevice
{
	Q_OBJECT

public:
	explicit LinuxInputDevice(QObject *parent = nullptr);
	explicit LinuxInputDevice(int fd, QObject *parent = nullptr);
	explicit LinuxInputDevice(const QString &name, QObject *parent = nullptr);
	explicit LinuxInputDevice(const LinuxInputDeviceInfo &inputDeviceInfo,
	                          QObject *parent = nullptr);
	~LinuxInputDevice() final;

public:
	bool isValid() const override;

private:
	bool openInputDevNode(const QString &path);

	void processEvents(const struct input_event *events, size_t nevents);

private slots:
	void onNotification(int fd);

private:
	friend class LinuxInputDeviceInfo;

	int m_fd;
	QSocketNotifier *m_notifier;

	qint32 m_scanCode;
};

#endif // LINUXINPUTDEVICE_H
