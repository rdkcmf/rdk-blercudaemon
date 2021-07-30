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
//  irdatabasepluginwrapper.cpp
//  SkyBluetoothRcu
//

#include "irdatabasepluginwrapper.h"
#include "utils/logging.h"

// -----------------------------------------------------------------------------
/*!
	Creates a new IrDatabasePluginWrapper object, which wraps a irdatabase qt plugin

	If plugin loading fails or when it's impossible to create a plugin's instance then an empty
	shared pointer is returned.

 */
IrDatabasePluginWrapper::IrDatabasePluginWrapper(const QString &pluginPath)
    : m_pluginLoader(pluginPath)
    , m_plugin(nullptr)
{
    if (m_pluginLoader.load())
    {
        m_plugin = qobject_cast<IrDatabase*>(m_pluginLoader.instance());

        if(m_plugin == nullptr)
        {
            qError() << "Failed to get ir database plugin instance " << m_pluginLoader.errorString();
        }
    }
    else
    {
        qError() << "Failed to load ir database plugin " << m_pluginLoader.errorString();
    }
}

IrDatabasePluginWrapper::~IrDatabasePluginWrapper()
{
    if (m_pluginLoader.isLoaded())
    {
        //this call will delete object created by QPluginLoader::instance
        if (!m_pluginLoader.unload())
        {
            qWarning("Failed to unload ir database plugin");
        }
    }
}

bool IrDatabasePluginWrapper::isValid() const
{
    if (m_plugin)
    {
        return m_plugin->isValid();
    }

    qWarning("%s failed. Plugin is nullptr", __FUNCTION__);
    return false;
}

QStringList IrDatabasePluginWrapper::brands(Type type, const QString& search,
    quint64* total, qint64 offset,
    qint64 limit) const
{
    if (m_plugin)
    {
        return m_plugin->brands(type, search, total, offset, limit);
    }
    
    qWarning("%s failed. Plugin is nullptr", __FUNCTION__);
    return QStringList();
}

QStringList IrDatabasePluginWrapper::models(Type type, const QString& brand,
    const QString& search, quint64* total,
    qint64 offset, qint64 limit) const
{
    if (m_plugin)
    {
        return m_plugin->models(type, brand, search, total, offset, limit);
    }

    qWarning("%s failed. Plugin is nullptr", __FUNCTION__);
    return QStringList();
}

QList<int> IrDatabasePluginWrapper::codeIds(Type type, const QString& brand,
    const QString& model) const
{
    if (m_plugin)
    {
        return m_plugin->codeIds(type, brand, model);
    }

    qWarning("%s failed. Plugin is nullptr", __FUNCTION__);
    return QList<int>();
}

QList<int> IrDatabasePluginWrapper::codeIds(const Edid& edid) const
{
    if (m_plugin)
    {
        return m_plugin->codeIds(edid);
    }

    qWarning("%s failed. Plugin is nullptr", __FUNCTION__);
    return QList<int>();
}

IrSignalSet IrDatabasePluginWrapper::irSignals(RcuType rcuType, int codeId) const
{
    if (m_plugin)
    {
        return m_plugin->irSignals(rcuType, codeId);
    }

    qWarning("%s failed. Plugin is nullptr", __FUNCTION__);
    return IrSignalSet();
}