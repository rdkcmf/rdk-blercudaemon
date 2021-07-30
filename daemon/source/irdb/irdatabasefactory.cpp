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
//  irdatabasefactory.cpp
//  SkyBluetoothRcu
//

#include "irdatabasefactory.h"
#ifdef USE_IR_DATABASE_PLUGIN
#include "irdatabasepluginwrapper.h"
#endif


// -----------------------------------------------------------------------------
/*!
	Creates a new IrDatabase object

 */
QSharedPointer<IrDatabase> IrDatabaseFactory::createDatabase(const QString &pluginPath)
{
    QSharedPointer<IrDatabase> db;

#ifdef USE_IR_DATABASE_PLUGIN
    db = QSharedPointer<IrDatabasePluginWrapper>::create(pluginPath);
#endif

    if (!db || !db->isValid())
        db.reset();

    return db;
}
