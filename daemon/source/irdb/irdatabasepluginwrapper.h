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
//  irdatabasepluginwrapper.h
//  SkyBluetoothRcu
//

#ifndef IRDATABASEPLUGINWRAPPER_H
#define IRDATABASEPLUGINWRAPPER_H

#include "irdatabase.h"
#include <QPluginLoader>

class IrDatabasePluginWrapper : public IrDatabase
{
public:
    IrDatabasePluginWrapper(const QString &pluginPath);
    ~IrDatabasePluginWrapper();

public:
    bool isValid() const override;

    QStringList brands(Type type, const QString& search,
        quint64* total = nullptr, qint64 offset = -1,
        qint64 limit = -1) const override;

    QStringList models(Type type, const QString& brand,
        const QString& search, quint64* total = nullptr,
        qint64 offset = -1, qint64 limit = -1) const override;

    QList<int> codeIds(Type type, const QString& brand,
        const QString& model = QStringLiteral("")) const override;

    QList<int> codeIds(const Edid& edid) const override;

    IrSignalSet irSignals(RcuType rcuType, int codeId) const override;

private:
    QPluginLoader m_pluginLoader;
    IrDatabase *m_plugin;

};

#endif // !defined(IRDATABASEPLUGINWRAPPER_H)