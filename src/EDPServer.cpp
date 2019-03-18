// Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
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
 * @file EDPServer.cpp
 *
 */

#include "EDPServer.h"
#include "EDPServerListeners.h"

#include <fastrtps/rtps/builtin/discovery/participant/PDP.h>
#include <rtps/participant/RTPSParticipantImpl.h>
#include <fastrtps/rtps/writer/StatefulWriter.h>
#include <fastrtps/rtps/reader/StatefulReader.h>
#include <fastrtps/rtps/attributes/HistoryAttributes.h>
#include <fastrtps/rtps/attributes/WriterAttributes.h>
#include <fastrtps/rtps/attributes/ReaderAttributes.h>
#include <fastrtps/rtps/history/ReaderHistory.h>
#include <fastrtps/rtps/history/WriterHistory.h>
#include <fastrtps/rtps/builtin/data/ParticipantProxyData.h>
#include <fastrtps/rtps/builtin/BuiltinProtocols.h>


#include <fastrtps/log/Log.h>

#include <mutex>

namespace eprosima {
namespace fastrtps{
namespace rtps {


bool EDPServer::createSEDPEndpoints()
{
    WriterAttributes watt;
    ReaderAttributes ratt;
    HistoryAttributes hatt;
    bool created = true;
    RTPSReader* raux = nullptr;
    RTPSWriter* waux = nullptr;

    publications_listener_ = new EDPServerPUBListener(this);
    subscriptions_listener_ = new EDPServerSUBListener(this);

    if(m_discovery.m_simpleEDP.use_PublicationWriterANDSubscriptionReader)
    {
        hatt.initialReservedCaches = edp_initial_reserved_caches;
        hatt.payloadMaxSize = DISCOVERY_PUBLICATION_DATA_MAX_SIZE;
        hatt.memoryPolicy = mp_PDP->mp_builtin->m_att.writerHistoryMemoryPolicy;
        publications_writer_.second = new WriterHistory(hatt);
        //Wparam.pushMode = true;
        watt.endpoint.reliabilityKind = RELIABLE;
        watt.endpoint.topicKind = WITH_KEY;
        watt.endpoint.unicastLocatorList = this->mp_PDP->getLocalParticipantProxyData()->m_metatrafficUnicastLocatorList;
        watt.endpoint.multicastLocatorList = this->mp_PDP->getLocalParticipantProxyData()->m_metatrafficMulticastLocatorList;
        //watt.endpoint.remoteLocatorList = m_discovery.initialPeersList;
        watt.endpoint.durabilityKind = TRANSIENT;
        watt.times.heartbeatPeriod = edp_heartbeat_period;
        watt.times.nackResponseDelay = edp_nack_response_delay;
        watt.times.nackSupressionDuration = edp_nack_supression_duration;
        if(mp_RTPSParticipant->getRTPSParticipantAttributes().throughputController.bytesPerPeriod != UINT32_MAX &&
                mp_RTPSParticipant->getRTPSParticipantAttributes().throughputController.periodMillisecs != 0)
            watt.mode = ASYNCHRONOUS_WRITER;

        created &=this->mp_RTPSParticipant->createWriter(&waux, watt, publications_writer_.second,
                publications_listener_, c_EntityId_SEDPPubWriter, true);

        if(created)
        {
            publications_writer_.first = dynamic_cast<StatefulWriter*>(waux);
            logInfo(RTPS_EDP,"SEDP Publication Writer created");
        }
        else
        {
            delete(publications_writer_.second);
            publications_writer_.second = nullptr;
        }

        hatt.initialReservedCaches = edp_initial_reserved_caches;
        hatt.payloadMaxSize = DISCOVERY_SUBSCRIPTION_DATA_MAX_SIZE;
        hatt.memoryPolicy = mp_PDP->mp_builtin->m_att.readerHistoryMemoryPolicy;
        subscriptions_reader_.second = new ReaderHistory(hatt);
        ratt.expectsInlineQos = false;
        ratt.endpoint.reliabilityKind = RELIABLE;
        ratt.endpoint.topicKind = WITH_KEY;
        ratt.endpoint.unicastLocatorList = this->mp_PDP->getLocalParticipantProxyData()->m_metatrafficUnicastLocatorList;
        ratt.endpoint.multicastLocatorList = this->mp_PDP->getLocalParticipantProxyData()->m_metatrafficMulticastLocatorList;
        //ratt.endpoint.remoteLocatorList = m_discovery.initialPeersList;
        ratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        ratt.times.heartbeatResponseDelay = edp_heartbeat_response_delay;

        created &=this->mp_RTPSParticipant->createReader(&raux, ratt, subscriptions_reader_.second,
                subscriptions_listener_, c_EntityId_SEDPSubReader, true);

        if(created)
        {
            subscriptions_reader_.first = dynamic_cast<StatefulReader*>(raux);
            logInfo(RTPS_EDP,"SEDP Subscription Reader created");
        }
        else
        {
            delete(subscriptions_reader_.second);
            subscriptions_reader_.second = nullptr;
        }
    }
    if(m_discovery.m_simpleEDP.use_PublicationReaderANDSubscriptionWriter)
    {
        hatt.initialReservedCaches = edp_initial_reserved_caches;
        hatt.payloadMaxSize = DISCOVERY_PUBLICATION_DATA_MAX_SIZE;
        hatt.memoryPolicy = mp_PDP->mp_builtin->m_att.readerHistoryMemoryPolicy;
        publications_reader_.second = new ReaderHistory(hatt);
        ratt.expectsInlineQos = false;
        ratt.endpoint.reliabilityKind = RELIABLE;
        ratt.endpoint.topicKind = WITH_KEY;
        ratt.endpoint.unicastLocatorList = this->mp_PDP->getLocalParticipantProxyData()->m_metatrafficUnicastLocatorList;
        ratt.endpoint.multicastLocatorList = this->mp_PDP->getLocalParticipantProxyData()->m_metatrafficMulticastLocatorList;
        //ratt.endpoint.remoteLocatorList = m_discovery.initialPeersList;
        ratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        ratt.times.heartbeatResponseDelay = edp_heartbeat_response_delay;

        created &=this->mp_RTPSParticipant->createReader(&raux, ratt, publications_reader_.second,
                publications_listener_, c_EntityId_SEDPPubReader, true);

        if(created)
        {
            publications_reader_.first = dynamic_cast<StatefulReader*>(raux);
            logInfo(RTPS_EDP,"SEDP Publication Reader created");

        }
        else
        {
            delete(publications_reader_.second);
            publications_reader_.second = nullptr;
        }

        hatt.initialReservedCaches = edp_initial_reserved_caches;
        hatt.payloadMaxSize = DISCOVERY_SUBSCRIPTION_DATA_MAX_SIZE;
        hatt.memoryPolicy = mp_PDP->mp_builtin->m_att.writerHistoryMemoryPolicy;
        subscriptions_writer_.second = new WriterHistory(hatt);
        //Wparam.pushMode = true;
        watt.endpoint.reliabilityKind = RELIABLE;
        watt.endpoint.topicKind = WITH_KEY;
        watt.endpoint.unicastLocatorList = this->mp_PDP->getLocalParticipantProxyData()->m_metatrafficUnicastLocatorList;
        watt.endpoint.multicastLocatorList = this->mp_PDP->getLocalParticipantProxyData()->m_metatrafficMulticastLocatorList;
        //watt.endpoint.remoteLocatorList = m_discovery.initialPeersList;
        watt.endpoint.durabilityKind = TRANSIENT;
        watt.times.heartbeatPeriod= edp_heartbeat_period;
        watt.times.nackResponseDelay = edp_nack_response_delay;
        watt.times.nackSupressionDuration = edp_nack_supression_duration;
        if(mp_RTPSParticipant->getRTPSParticipantAttributes().throughputController.bytesPerPeriod != UINT32_MAX &&
                mp_RTPSParticipant->getRTPSParticipantAttributes().throughputController.periodMillisecs != 0)
            watt.mode = ASYNCHRONOUS_WRITER;

        created &=this->mp_RTPSParticipant->createWriter(&waux, watt, subscriptions_writer_.second,
                subscriptions_listener_, c_EntityId_SEDPSubWriter, true);

        if(created)
        {
            subscriptions_writer_.first = dynamic_cast<StatefulWriter*>(waux);
            logInfo(RTPS_EDP,"SEDP Subscription Writer created");

        }
        else
        {
            delete(subscriptions_writer_.second);
            subscriptions_writer_.second = nullptr;
        }
    }
    logInfo(RTPS_EDP,"Creation finished");
    return created;
}

} /* namespace rtps */
} /* namespace fastrtps */
} /* namespace eprosima */
