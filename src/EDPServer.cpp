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

#include "PDPServer.h"
// #include <fastrtps/rtps/builtin/discovery/participant/PDP.h>
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
#include <forward_list>

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

    PDPServer * pPDP = dynamic_cast<PDPServer*>(mp_PDP);
    assert(pPDP);

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
        watt.endpoint.properties.properties().push_back(Property("dds.persistence.plugin", "builtin.SQLITE3"));
        watt.endpoint.properties.properties().push_back(Property("dds.persistence.sqlite3.filename", pPDP->GetPersistenceFileName()));
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
        watt.endpoint.properties.properties().push_back(Property("dds.persistence.plugin", "builtin.SQLITE3"));
        watt.endpoint.properties.properties().push_back(Property("dds.persistence.sqlite3.filename", pPDP->GetPersistenceFileName()));
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

template<class ProxyCont> 
void EDPServer::trimWriterHistory(key_list & _demises, StatefulWriter & writer, WriterHistory & history, ProxyCont ParticipantProxyData::* pC)
{
    // trim demises container
    key_list disposal, aux;

    if (_demises.empty())
        return;

    std::lock_guard<std::recursive_mutex> guardP(*mp_PDP->getMutex());

    // sweep away any resurrected endpoint
    for (auto iD = mp_PDP->ParticipantProxiesBegin(); iD != mp_PDP->ParticipantProxiesEnd(); ++iD)
    {
        ProxyCont & readers = (*iD)->*pC;

        for (auto iE : readers)
        {
            disposal.insert(iE->key());
        }
    }
    std::set_difference(_demises.cbegin(), _demises.cend(), disposal.cbegin(), disposal.cend(),
        std::inserter(aux, aux.begin()));
    _demises.swap(aux);

    if (_demises.empty())
        return;

    // traverse the WriterHistory searching CacheChanges_t with demised keys  
    std::forward_list<CacheChange_t*> removal;
    std::lock_guard<std::recursive_mutex> guardW(*writer.getMutex());

    std::copy_if(history.changesBegin(), history.changesBegin(), std::front_inserter(removal),
        [_demises](const CacheChange_t* chan) { return _demises.find(chan->instanceHandle) != _demises.cend();  });

    if (removal.empty())
        return;

    aux.clear();
    key_list & pending = aux;

    // remove outdate CacheChange_ts
    for (auto pC : removal)
    {
        if (writer.is_acked_by_all(pC))
            history.remove_change(pC);
        else
            pending.insert(pC->instanceHandle);
    }

    // update demises
    _demises.swap(pending);

}

bool EDPServer::addEndpointFromHistory(StatefulWriter & writer, WriterHistory & history, const CacheChange_t & c)
{
    std::lock_guard<std::recursive_mutex> guardW(*writer.getMutex());
    CacheChange_t * pCh = nullptr;

    // history.reserve_Cache(&pCh, DISCOVERY_PUBLICATION_DATA_MAX_SIZE )
    // history.reserve_Cache(&pCh, DISCOVERY_SUBSCRIPTION_DATA_MAX_SIZE )
    if (history.reserve_Cache(&pCh, c.serializedPayload.max_size) && pCh && pCh->copy(&c))
    {
        pCh->writerGUID = writer.getGuid();
        return history.add_change(pCh, pCh->write_params);
    }

    return false;
}

void EDPServer::removePublisherFromHistory(const InstanceHandle_t & key)
{
    std::lock_guard<std::recursive_mutex> guardP(*mp_PDP->getMutex());

    _PUBdemises.insert(key);
    trimPUBWriterHistory();

}

void EDPServer::removeSubscriberFromHistory(const InstanceHandle_t & key)
{
    std::lock_guard<std::recursive_mutex> guardP(*mp_PDP->getMutex());

    _SUBdemises.insert(key);
    trimSUBWriterHistory();
}


} /* namespace rtps */
} /* namespace fastrtps */
} /* namespace eprosima */
