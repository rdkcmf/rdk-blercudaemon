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

#include "btrmgradapter.h"

#include "utils/logging.h"


#include <btmgr.h>

#include <thread>
#include <chrono>

#define LOG_IF_FAILED(func, ... ) 															\
			do {																			\
				auto __result = func(__VA_ARGS__);											\
				if(__result != BTRMGR_RESULT_SUCCESS)											\
				{																			\
					qError("call to %s failed with result %d", #func, (int)__result);			\
				}																			\
			}while(false)


namespace
{
	unsigned char getNumberOfAdapters() noexcept
	{
		unsigned char result{};
		LOG_IF_FAILED(BTRMGR_GetNumberOfAdapters, &result);
		return result;
	}

	struct DiscoveryState
	{
		BTRMGR_DiscoveryStatus_t status = BTRMGR_DISCOVERY_STATUS_OFF;
		BTRMGR_DeviceOperationType_t operationType = BTRMGR_DEVICE_OP_TYPE_UNKNOWN;
	};

	DiscoveryState getDiscoveryState(unsigned char adapterIdx) noexcept
	{
		auto state = DiscoveryState{};

		LOG_IF_FAILED(BTRMGR_GetDiscoveryStatus, adapterIdx, &state.status, &state.operationType);

		return state;
	}
}

BtrMgrAdapter::ApiInitializer::ApiInitializer() noexcept
{
	LOG_IF_FAILED(BTRMGR_Init);
}

BtrMgrAdapter::ApiInitializer::~ApiInitializer() noexcept
{
	LOG_IF_FAILED(BTRMGR_DeInit);
}

BtrMgrAdapter::BtrMgrAdapter() noexcept
{
	const auto numOfAdapters = getNumberOfAdapters();
	adapterIdx = numOfAdapters-1;
}

void BtrMgrAdapter::startDiscovery(BtrMgrAdapter::OperationType requestedOperationType) const noexcept
{
	LOG_IF_FAILED(BTRMGR_StartDeviceDiscovery, adapterIdx, requestedOperationType);
}

BtrMgrAdapter::OperationType BtrMgrAdapter::stopDiscovery() const noexcept
{
	const auto state = getDiscoveryState(adapterIdx);

	LOG_IF_FAILED(BTRMGR_StopDeviceDiscovery, adapterIdx, state.operationType);
	return state.operationType;
}

bool BtrMgrAdapter::isDiscoveryInProgress() const noexcept
{
	return getDiscoveryState(adapterIdx).status  == BTRMGR_DISCOVERY_STATUS_IN_PROGRESS;
}
