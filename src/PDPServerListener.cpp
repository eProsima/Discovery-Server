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
 * @file PDPServerListener.cpp
 *
 */

#include <fastrtps/rtps/reader/RTPSReader.h>

#include <fastrtps/rtps/history/ReaderHistory.h>

#include <fastrtps/rtps/builtin/data/ParticipantProxyData.h>

#include <fastrtps/utils/TimeConversion.h>

#include <fastrtps/rtps/builtin/discovery/participant/timedevent/RemoteParticipantLeaseDuration.h>

#include <fastrtps/rtps/participant/ParticipantDiscoveryInfo.h>
#include <fastrtps/rtps/participant/RTPSParticipantListener.h>

#include <rtps/participant/RTPSParticipantImpl.h>

#include <mutex>

#include <fastrtps/log/Log.h>

#include "PDPServerListener.h"
#include "PDPServer.h"

namespace eprosima {
namespace fastrtps{
namespace rtps {



void PDPServerListener::onNewCacheChangeAdded(RTPSReader* reader, const CacheChange_t* const change_in)
{
    CacheChange_t* change = (CacheChange_t*)(change_in);
    logInfo(RTPS_PDP,"SPDP Message received");
    if(change->instanceHandle == c_InstanceHandle_Unknown)
    {
        if(!this->getKey(change))
        {
            logWarning(RTPS_PDP,"Problem getting the key of the change, removing");
            this->mp_PDP->mp_PDPReaderHistory->remove_change(change);
            return;
        }
    }
    if(change->kind == ALIVE)
    {
        //LOAD INFORMATION IN TEMPORAL RTPSParticipant PROXY DATA
        ParticipantProxyData participant_data;
        CDRMessage_t msg(change->serializedPayload);
        if(participant_data.readFromCDRMessage(&msg))
        {
            //AFTER CORRECTLY READING IT
            //CHECK IF IS THE SAME RTPSParticipant
            change->instanceHandle = participant_data.m_key;
            if(participant_data.m_guid == mp_PDP->getRTPSParticipant()->getGuid())
            {
                logInfo(RTPS_PDP,"Message from own RTPSParticipant, removing");
                this->mp_PDP->mp_PDPReaderHistory->remove_change(change);
                return;
            }

            // At this point we can release reader lock.
            reader->getMutex()->unlock();

            //LOOK IF IS AN UPDATED INFORMATION
            ParticipantProxyData* pdata = nullptr;
            std::unique_lock<std::recursive_mutex> lock(*mp_PDP->getMutex());
            for (auto it = mp_PDP->m_participantProxies.begin();
                    it != mp_PDP->m_participantProxies.end();++it)
            {
                if(participant_data.m_key == (*it)->m_key)
                {
                    pdata = (*it);
                    break;
                }
            }

            auto status = (pdata == nullptr) ? ParticipantDiscoveryInfo::DISCOVERED_PARTICIPANT :
                ParticipantDiscoveryInfo::CHANGED_QOS_PARTICIPANT;

            if(pdata == nullptr)
            {
                //IF WE DIDNT FOUND IT WE MUST CREATE A NEW ONE
                pdata = new ParticipantProxyData(participant_data);
                pdata->isAlive = true;
                pdata->mp_leaseDurationTimer = new RemoteParticipantLeaseDuration(mp_PDP,
                        pdata,
                        TimeConv::Time_t2MilliSecondsDouble(pdata->m_leaseDuration));
                pdata->mp_leaseDurationTimer->restart_timer();
                this->mp_PDP->m_participantProxies.push_back(pdata);
                lock.unlock();

                mp_PDP->announceParticipantState(false);
                mp_PDP->assignRemoteEndpoints(pdata);
                mp_PDP->queueParticipantForEDPMatch(pdata);
            }
            else
            {
                pdata->updateData(participant_data);
                pdata->isAlive = true;
                lock.unlock();

                if (mp_PDP->updateInfoMatchesEDP())
                    mp_PDP->mp_EDP->assignRemoteEndpoints(*pdata);

            }

            // update the PDP Writer with this reader info
            if(!mp_PDP->addParticipantToHistory(*change))
            {
                logError(RTPS_PDP, "Unable to update the PDP Writer from PDPServerListener");
            }

            auto listener = this->mp_PDP->getRTPSParticipant()->getListener();
            if (listener != nullptr)
            {
                ParticipantDiscoveryInfo info;
                info.status = status;
                info.info = participant_data;

                listener->onParticipantDiscovery(this->mp_PDP->getRTPSParticipant()->getUserRTPSParticipant(), std::move(info));
            }

            // Take again the reader lock
            reader->getMutex()->lock();
        }
    }
    else
    {
        GUID_t guid;
        iHandle2GUID(guid, change->instanceHandle);

        ParticipantDiscoveryInfo info;
        info.status = ParticipantDiscoveryInfo::REMOVED_PARTICIPANT;

        this->mp_PDP->lookupParticipantProxyData(guid, info.info);

        if(this->mp_PDP->removeRemoteParticipant(guid))
        {
            auto listener = this->mp_PDP->getRTPSParticipant()->getListener();
            if(listener != nullptr)
            {
                listener->onParticipantDiscovery(this->mp_PDP->getRTPSParticipant()->getUserRTPSParticipant(), std::move(info));
            }
        }

        assert(change->instanceHandle == info.info.m_key);
        mp_PDP->removeParticipantFromHistory(change->instanceHandle);
        mp_PDP->removeParticipantForEDPMatch(&info.info);

    }

    //Remove change form history.
    this->mp_PDP->mp_PDPReaderHistory->remove_change(change);

    return;
}

bool PDPServerListener::getKey(CacheChange_t* change)
{
    return ParameterList::readInstanceHandleFromCDRMsg(change, PID_PARTICIPANT_GUID);
}

}
} /* namespace rtps */
} /* namespace eprosima */
