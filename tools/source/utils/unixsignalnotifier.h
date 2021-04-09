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
//  unixsignalnotifier.h
//  SkyBluetoothRcu
//

#ifndef UNIXSIGNALNOTIFIER_H
#define UNIXSIGNALNOTIFIER_H

#include <QObject>


class UnixSignalNotifier : public QObject
{
	Q_OBJECT

public:
	explicit UnixSignalNotifier(int unixSignal, QObject *parent = nullptr);
	~UnixSignalNotifier() final;

	bool isEnabled() const;
	int unixSignal() const;

signals:
	void activated(int unixSignal);

public slots:
	void setEnabled(bool enable);

private slots:
	void onSignalActivated(int unixSignal);

private:
	const int m_unixSignal;
	bool m_enabled;

private:
	Q_DISABLE_COPY(UnixSignalNotifier)
};

#endif // !defined(UNIXSIGNALNOTIFIER_H)
