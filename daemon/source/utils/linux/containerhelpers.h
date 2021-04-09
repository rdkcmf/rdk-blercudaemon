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
//  containerhelpers.h
//  BleRcuDaemon
//

#ifndef CONTAINERHELPERS_H
#define CONTAINERHELPERS_H

#include <utility>
#include <functional>

#include <sys/types.h>


pid_t getRealProcessId();

int createSocketInNs(int netNsFd, int domain, int type, int protocol);



bool runInNetworkNamespaceImpl(int netNsFd, const std::function<void()> &f);

template< class Function >
static inline bool runInNetworkNamespace(int netNsFd, Function f)
{
	return runInNetworkNamespaceImpl(netNsFd, f);
}

template< class Function, class... Args >
static inline bool runInNetworkNamespace(int netNsFd, Function&& f, Args&&... args)
{
	return runInNetworkNamespaceImpl(netNsFd, std::bind(std::forward<Function>(f),
	                                                    std::forward<Args>(args)...));
}



#endif // !defined(CONTAINERHELPERS_H)
