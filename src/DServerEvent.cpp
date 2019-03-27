// Copyright 2019 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file DServerEvent.cpp
 *
 */

#include <fastrtps/rtps/builtin/data/ParticipantProxyData.h>

#include <fastrtps/rtps/resources/ResourceEvent.h>

#include <rtps/participant/RTPSParticipantImpl.h>

#include <fastrtps/log/Log.h>

#include "DServerEvent.h"
#include "PDPServer.h"


namespace eprosima {
namespace fastrtps{
namespace rtps {


DServerEvent::DServerEvent(PDPServer* p_PDP,
        double interval):
    TimedEvent(p_PDP->getRTPSParticipant()->getEventResource().getIOService(),
            p_PDP->getRTPSParticipant()->getEventResource().getThread(), interval),
    mp_PDP(p_PDP)
    {

    }

DServerEvent::~DServerEvent()
{
    destroy();
}

void DServerEvent::event(EventCode code, const char* msg)
{
    // Unused in release mode.
    (void)msg;

    if(code == EVENT_SUCCESS)
    {
        logInfo(RTPS_PDP,"DServerEvent Period");

        std::lock_guard<std::recursive_mutex> lock(*mp_PDP->getMutex());

        if (mp_PDP->pendingEDPMatches() && mp_PDP->all_clients_acknowledge_PDP())
        {
            // Do the matching
            mp_PDP->match_all_clients_EDP_endpoints();
        }

        restart_timer();
    }
    else if(code == EVENT_ABORT)
    {
        logInfo(RTPS_PDP,"DServerEvent aborted");
    }
    else
    {
        logInfo(RTPS_PDP,"message: " <<msg);
    }
}

}
} /* namespace rtps */
} /* namespace eprosima */
