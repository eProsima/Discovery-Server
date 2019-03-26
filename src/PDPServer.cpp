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
 * @file PDPServer.cpp
 *
 */

#include <fastrtps/rtps/builtin/BuiltinProtocols.h>
#include <fastrtps/rtps/builtin/liveliness/WLP.h>

#include <fastrtps/rtps/participant/RTPSParticipantListener.h>
#include <fastrtps/rtps/reader/StatefulReader.h>

#include <fastrtps/rtps/history/WriterHistory.h>
#include <fastrtps/rtps/history/ReaderHistory.h>

#include <fastrtps/utils/TimeConversion.h>

#include <rtps/participant/RTPSParticipantImpl.h>

#include <fastrtps/log/Log.h>

#include "DServerEvent.h"
#include "PDPServerListener.h"
#include "PDPServer.h"
#include "EDPServer.h"

#include <algorithm>
#include <forward_list>

using namespace eprosima::fastrtps;

namespace eprosima {
namespace fastrtps{
namespace rtps {

PDPServer::PDPServer(BuiltinProtocols* built):
    PDP(built), mp_sync(nullptr)
    {

    }

PDPServer::~PDPServer()
{
    if (mp_sync != nullptr)
        delete(mp_sync);
}

bool PDPServer::initPDP(RTPSParticipantImpl* part)
{
    if (!PDP::initPDP(part))
    {
        return false;
    }

    //INIT EDP
    mp_EDP = (EDP*)(new EDPServer(this, mp_RTPSParticipant));
    if (!mp_EDP->initEDP(m_discovery))
    {
        logError(RTPS_PDP, "Endpoint discovery configuration failed");
        return false;
    }

    /*
        Given the fact that a participant is either a client or a server the 
        discoveryServer_client_syncperiod parameter has a context defined meaning.
    */
    mp_sync = new DServerEvent(this, TimeConv::Time_t2MilliSecondsDouble(m_discovery.discoveryServer_client_syncperiod));

    return true;
}

bool PDPServer::createPDPEndpoints()
{
    logInfo(RTPS_PDP, "Beginning PDPServer Endpoints creation");

    HistoryAttributes hatt;
    hatt.payloadMaxSize = DISCOVERY_PARTICIPANT_DATA_MAX_SIZE;
    hatt.initialReservedCaches = pdp_initial_reserved_caches;
    hatt.memoryPolicy = mp_builtin->m_att.readerHistoryMemoryPolicy;
    mp_PDPReaderHistory = new ReaderHistory(hatt);

    ReaderAttributes ratt;
    ratt.expectsInlineQos = false;
    ratt.endpoint.endpointKind = READER;
    ratt.endpoint.multicastLocatorList = mp_builtin->m_metatrafficMulticastLocatorList;
    ratt.endpoint.unicastLocatorList = mp_builtin->m_metatrafficUnicastLocatorList;
    ratt.endpoint.topicKind = WITH_KEY;
    ratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
    ratt.endpoint.reliabilityKind = RELIABLE;
    ratt.times.heartbeatResponseDelay = pdp_heartbeat_response_delay;

    mp_listener = new PDPServerListener(this);

    if (mp_RTPSParticipant->createReader(&mp_PDPReader, ratt, mp_PDPReaderHistory, mp_listener, c_EntityId_SPDPReader, true, false))
    {
        // enable unknown clients to reach this reader
        mp_PDPReader->enableMessagesFromUnkownWriters(true);

        for (auto it = mp_builtin->m_DiscoveryServers.begin(); it != mp_builtin->m_DiscoveryServers.end(); ++it)
        {
            RemoteWriterAttributes rwatt;

            rwatt.guid = it->GetPDPWriter();
            rwatt.endpoint.multicastLocatorList.push_back(it->metatrafficMulticastLocatorList);
            rwatt.endpoint.unicastLocatorList.push_back(it->metatrafficUnicastLocatorList);
            rwatt.endpoint.topicKind = WITH_KEY;
            rwatt.endpoint.durabilityKind = TRANSIENT; // Server Information must be persistent
            rwatt.endpoint.reliabilityKind = RELIABLE;

            // TODO: remove the join when Reader and Writer match functions are updated
            rwatt.endpoint.remoteLocatorList.push_back(it->metatrafficMulticastLocatorList);
            rwatt.endpoint.remoteLocatorList.push_back(it->metatrafficUnicastLocatorList);

            mp_PDPReader->matched_writer_add(rwatt);
        }

    }
    else
    {
        logError(RTPS_PDP, "PDPServer Reader creation failed");
        delete(mp_PDPReaderHistory);
        mp_PDPReaderHistory = nullptr;
        delete(mp_listener);
        mp_listener = nullptr;
        return false;
    }

    hatt.payloadMaxSize = DISCOVERY_PARTICIPANT_DATA_MAX_SIZE;
    hatt.initialReservedCaches = pdp_initial_reserved_caches;
    hatt.memoryPolicy = mp_builtin->m_att.writerHistoryMemoryPolicy;
    mp_PDPWriterHistory = new WriterHistory(hatt);

    WriterAttributes watt;
    watt.endpoint.endpointKind = WRITER;
    watt.endpoint.durabilityKind = TRANSIENT;
    watt.endpoint.properties.properties().push_back(Property("dds.persistence.plugin", "builtin.SQLITE3"));
    watt.endpoint.properties.properties().push_back(Property("dds.persistence.sqlite3.filename", GetPersistenceFileName()));
    watt.endpoint.reliabilityKind = RELIABLE;
    watt.endpoint.topicKind = WITH_KEY;
    watt.endpoint.multicastLocatorList = mp_builtin->m_metatrafficMulticastLocatorList;
    watt.endpoint.unicastLocatorList = mp_builtin->m_metatrafficUnicastLocatorList;
    watt.times.heartbeatPeriod = pdp_heartbeat_period;
    watt.times.nackResponseDelay = pdp_nack_response_delay;
    watt.times.nackSupressionDuration = pdp_nack_supression_duration;

    if (mp_RTPSParticipant->getRTPSParticipantAttributes().throughputController.bytesPerPeriod != UINT32_MAX &&
        mp_RTPSParticipant->getRTPSParticipantAttributes().throughputController.periodMillisecs != 0)
    {
        watt.mode = ASYNCHRONOUS_WRITER;
    }

    if (mp_RTPSParticipant->createWriter(&mp_PDPWriter, watt, mp_PDPWriterHistory, nullptr, c_EntityId_SPDPWriter, true))
    {

        for (auto it = mp_builtin->m_DiscoveryServers.begin(); it != mp_builtin->m_DiscoveryServers.end(); ++it)
        {
            RemoteReaderAttributes rratt;

            rratt.guid = it->GetPDPReader();
            rratt.endpoint.multicastLocatorList.push_back(it->metatrafficMulticastLocatorList);
            rratt.endpoint.unicastLocatorList.push_back(it->metatrafficUnicastLocatorList);
            rratt.endpoint.topicKind = WITH_KEY;
            rratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
            rratt.endpoint.reliabilityKind = RELIABLE;

            // TODO: remove the join when Reader and Writer match functions are updated
            rratt.endpoint.remoteLocatorList.push_back(it->metatrafficMulticastLocatorList);
            rratt.endpoint.remoteLocatorList.push_back(it->metatrafficUnicastLocatorList);

            mp_PDPWriter->matched_reader_add(rratt);
        }

    }
    else
    {
        logError(RTPS_PDP, "PDPServer Writer creation failed");
        delete(mp_PDPWriterHistory);
        mp_PDPWriterHistory = nullptr;
        return false;
    }
    logInfo(RTPS_PDP, "PDPServer Endpoints creation finished");
    return true;
}

void PDPServer::initializeParticipantProxyData(ParticipantProxyData* participant_data)
{
    PDP::initializeParticipantProxyData(participant_data); // TODO: Remember that the PDP version USES security

    if (!(getRTPSParticipant()->getAttributes().builtin.discoveryProtocol != PDPType_t::CLIENT))
    {
        logError(RTPS_PDP, "Using a PDP Server object with another user's settings");
    }

    if (getRTPSParticipant()->getAttributes().builtin.m_simpleEDP.use_PublicationWriterANDSubscriptionReader)
    {
        participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER;
        participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_DETECTOR;
    }

    if (getRTPSParticipant()->getAttributes().builtin.m_simpleEDP.use_PublicationReaderANDSubscriptionWriter)
    {
        participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_PUBLICATION_DETECTOR;
        participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER;
    }
}


void PDPServer::assignRemoteEndpoints(ParticipantProxyData* pdata)
{
    // Verify if this participant is a server
    for (auto svr : mp_builtin->m_DiscoveryServers)
    {
        if (svr.guidPrefix == pdata->m_guid.guidPrefix)
        {
            svr.proxy = pdata;
            // servers are already match in PDPServer::createPDPEndpoints
            // Notify another endpoints
            notifyAboveRemoteEndpoints(*pdata);
            return;
        }
    }

    // boilerplate, note that PDPSimple version doesn't use RELIABLE entities
    logInfo(RTPS_PDP, "For RTPSParticipant: " << pdata->m_guid.guidPrefix);
    uint32_t endp = pdata->m_availableBuiltinEndpoints;
    uint32_t auxendp = endp;
    auxendp &= DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER;
    if (auxendp != 0)
    {
        RemoteWriterAttributes watt(pdata->m_VendorId);
        watt.guid.guidPrefix = pdata->m_guid.guidPrefix;
        watt.guid.entityId = c_EntityId_SPDPWriter;
        watt.endpoint.persistence_guid = watt.guid;
        watt.endpoint.unicastLocatorList = pdata->m_metatrafficUnicastLocatorList;
        watt.endpoint.multicastLocatorList = pdata->m_metatrafficMulticastLocatorList;
        watt.endpoint.reliabilityKind = RELIABLE;
        watt.endpoint.durabilityKind = TRANSIENT_LOCAL; 

        // TODO remove the join when Reader and Writer match functions are updated
        watt.endpoint.remoteLocatorList.push_back(pdata->m_metatrafficUnicastLocatorList);
        watt.endpoint.remoteLocatorList.push_back(pdata->m_metatrafficMulticastLocatorList);

        mp_PDPReader->matched_writer_add(watt);
    }
    auxendp = endp;
    auxendp &= DISC_BUILTIN_ENDPOINT_PARTICIPANT_DETECTOR;
    if (auxendp != 0)
    {
        RemoteReaderAttributes ratt(pdata->m_VendorId);
        ratt.expectsInlineQos = false;
        ratt.guid.guidPrefix = pdata->m_guid.guidPrefix;
        ratt.guid.entityId = c_EntityId_SPDPReader;
        ratt.endpoint.unicastLocatorList = pdata->m_metatrafficUnicastLocatorList;
        ratt.endpoint.multicastLocatorList = pdata->m_metatrafficMulticastLocatorList;
        ratt.endpoint.reliabilityKind = RELIABLE;
        ratt.endpoint.durabilityKind = TRANSIENT_LOCAL;

        // TODO remove the join when Reader and Writer match functions are updated
        ratt.endpoint.remoteLocatorList.push_back(pdata->m_metatrafficUnicastLocatorList);
        ratt.endpoint.remoteLocatorList.push_back(pdata->m_metatrafficMulticastLocatorList);

        mp_PDPWriter->matched_reader_add(ratt);
    }

    // Notify another endpoints
    notifyAboveRemoteEndpoints(*pdata);

}

void PDPServer::notifyAboveRemoteEndpoints(const ParticipantProxyData& pdata)
{
    // No EDP notification needed. EDP endpoints would be match when PDP synchronization is granted
    if (mp_builtin->mp_WLP != nullptr)
        mp_builtin->mp_WLP->assignRemoteEndpoints(pdata);
}


void PDPServer::removeRemoteEndpoints(ParticipantProxyData* pdata)
{
    // EDP endpoints have been already unmatch by the associated listener
    assert(!mp_EDP->areRemoteEndpointsMatched(pdata));

    // Verify if this participant is a server
    bool is_server = false;
    for (auto svr : mp_builtin->m_DiscoveryServers)
    {
        if (svr.guidPrefix == pdata->m_guid.guidPrefix)
        {
            svr.proxy = nullptr; // reasign when we receive again server DATA(p)
            is_server = true;
        }
    }

    // Clients should be unmatch and
    // servers unmatch and match in order to renew its associated proxies
    logInfo(RTPS_PDP, "For unmatching for server: " << pdata->m_guid);
    uint32_t endp = pdata->m_availableBuiltinEndpoints;
    uint32_t auxendp = endp;
    auxendp &= DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER;

    if (auxendp != 0)
    {
        RemoteWriterAttributes watt;
    
        watt.guid.guidPrefix = pdata->m_guid.guidPrefix;
        watt.guid.entityId = c_EntityId_SPDPWriter;
        watt.endpoint.persistence_guid = watt.guid;
        watt.endpoint.unicastLocatorList = pdata->m_metatrafficUnicastLocatorList;
        watt.endpoint.multicastLocatorList = pdata->m_metatrafficMulticastLocatorList;
        watt.endpoint.reliabilityKind = RELIABLE;
        watt.endpoint.durabilityKind = is_server ? TRANSIENT : TRANSIENT_LOCAL;
        watt.endpoint.topicKind = WITH_KEY;

        mp_PDPReader->matched_writer_remove(watt);

        if (is_server)
        {
            mp_PDPReader->matched_writer_add(watt);
        }

    }

    auxendp = endp;
    auxendp &= DISC_BUILTIN_ENDPOINT_PARTICIPANT_DETECTOR;

    if (auxendp != 0)
    {
        RemoteReaderAttributes ratt;

        ratt.expectsInlineQos = false;
        ratt.guid.guidPrefix = pdata->m_guid.guidPrefix;
        ratt.guid.entityId = c_EntityId_SPDPReader;
        ratt.endpoint.unicastLocatorList = pdata->m_metatrafficUnicastLocatorList;
        ratt.endpoint.multicastLocatorList = pdata->m_metatrafficMulticastLocatorList;
        ratt.endpoint.reliabilityKind = RELIABLE;
        ratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        ratt.endpoint.topicKind = WITH_KEY;

        mp_PDPWriter->matched_reader_remove(ratt);

        if (is_server)
        {
            mp_PDPWriter->matched_reader_add(ratt);
        }
    }
    
}

bool PDPServer::all_clients_acknowledge_PDP()
{
    // check if already initialized
    assert( mp_PDPWriter && dynamic_cast<StatefulWriter*>(mp_PDPWriter));

    return dynamic_cast<StatefulWriter*>(mp_PDPWriter)->all_readers_updated();
}


void PDPServer::match_all_clients_EDP_endpoints()
{
    // PDP must have been initialize
    assert(mp_EDP);

    std::lock_guard<std::recursive_mutex> guardPDP(*mp_mutex);

    if (!pendingEDPMatches())
        return;
    
    for (auto p: _p2match)
    {
       assert( p != nullptr);
       mp_EDP->assignRemoteEndpoints(*p);
    }

    _p2match.clear();
}

void PDPServer::trimWriterHistory()
{
    assert(mp_mutex && mp_PDPWriter && mp_PDPWriter->getMutex());

    // trim demises container
    key_list disposal, aux;

    if (_demises.empty())
        return;

    std::lock_guard<std::recursive_mutex> guardP(*mp_mutex);
    
    // sweep away any resurrected participant
    std::for_each(ParticipantProxiesBegin(), ParticipantProxiesEnd(),
        [&disposal](const ParticipantProxyData* pD) { disposal.insert(pD->m_key); });
    std::set_difference(_demises.cbegin(), _demises.cend(), disposal.cbegin(), disposal.cend(),
        std::inserter(aux,aux.begin()));
    _demises.swap(aux);

    if (_demises.empty())
        return;

    // traverse the WriterHistory searching CacheChanges_t with demised keys  
    std::forward_list<CacheChange_t*> removal;
    std::lock_guard<std::recursive_mutex> guardW(*mp_PDPWriter->getMutex());

    std::copy_if(mp_PDPWriterHistory->changesBegin(), mp_PDPWriterHistory->changesBegin(), std::front_inserter(removal),
        [this](const CacheChange_t* chan) { return _demises.find(chan->instanceHandle) != _demises.cend();  });

    if (removal.empty())
        return;

    aux.clear();
    key_list & pending = aux;

    // remove outdate CacheChange_ts
    for (auto pC : removal)
    {
        if (mp_PDPWriter->is_acked_by_all(pC))
            mp_PDPWriterHistory->remove_change(pC);
        else
            pending.insert(pC->instanceHandle);
    }

    // update demises
    _demises.swap(pending);
}

bool PDPServer::addParticipantToHistory(const CacheChange_t & c)
{
    assert(mp_PDPWriter && mp_PDPWriter->getMutex() && c.serializedPayload.max_size);

    std::lock_guard<std::recursive_mutex> guardW(*mp_PDPWriter->getMutex());
    CacheChange_t * pCh = nullptr;

    // mp_PDPWriterHistory->reserve_Cache(&pCh, DISCOVERY_PARTICIPANT_DATA_MAX_SIZE)
    if (mp_PDPWriterHistory->reserve_Cache(&pCh, c.serializedPayload.max_size) && pCh && pCh->copy(&c) )
    {
        pCh->writerGUID = mp_PDPWriter->getGuid();
        return mp_PDPWriterHistory->add_change(pCh,pCh->write_params);
    }

    return false;
}

// Always call after PDP proxies update
void PDPServer::removeParticipantFromHistory(const InstanceHandle_t & key)
{
    std::lock_guard<std::recursive_mutex> guardP(*mp_mutex);

    _demises.insert(key);
    trimWriterHistory(); // first call, TODO: DServerEvent should keep calling
}

void PDPServer::queueParticipantForEDPMatch(const ParticipantProxyData * pdata)
{
    assert(pdata != nullptr);

    std::lock_guard<std::recursive_mutex> guardP(*mp_mutex);

    // add the new client or server to the EDP matching list
    _p2match.insert(pdata);
}


void PDPServer::removeParticipantForEDPMatch(const ParticipantProxyData * pdata)
{
    assert(pdata != nullptr);

    std::lock_guard<std::recursive_mutex> guardP(*mp_mutex);

    // servers cannot die so shouldn't be removed
    for( auto svr: mp_builtin->m_DiscoveryServers)
    {
        if (svr.guidPrefix == pdata->m_guid.guidPrefix)
        {
            assert(svr.proxy != nullptr); // should be already registered
            return;
        }
    }

    // remove the deceased client to the EDP matching list
    _p2match.erase(pdata);
}

std::string PDPServer::GetPersistenceFileName() 
{
    assert(getRTPSParticipant());

   std::ostringstream filename(std::ios_base::ate);
   std::string prefix;

   // . is not suitable separator for filenames
   filename << "server-" << getRTPSParticipant()->getGuid().guidPrefix;
   prefix = filename.str();
   std::replace(prefix.begin(), prefix.end(), '.', '-');
   filename.str(std::move(prefix));
   filename << ".db";

   return filename.str();

}


} /* namespace rtps */
} /* namespace fastrtps */
} /* namespace eprosima */
