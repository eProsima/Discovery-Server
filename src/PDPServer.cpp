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

#include <fastrtps/rtps/builtin/discovery/participant/PDPListener.h>
#include <fastrtps/rtps/builtin/BuiltinProtocols.h>

#include <fastrtps/rtps/participant/RTPSParticipantListener.h>
#include <fastrtps/rtps/reader/StatefulReader.h>

#include <fastrtps/rtps/history/WriterHistory.h>
#include <fastrtps/rtps/history/ReaderHistory.h>

#include <fastrtps/utils/TimeConversion.h>

#include <rtps/participant/RTPSParticipantImpl.h>

#include <fastrtps/log/Log.h>

#include "DServerEvent.h"
#include "PDPServer.h"


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
    mp_EDP = (EDP*)(new EDPSimple(this, mp_RTPSParticipant));
    if (!mp_EDP->initEDP(m_discovery))
    {
        logError(RTPS_PDP, "Endpoint discovery configuration failed");
        return false;
    }

    /*
        TODO: think syncperiod over.
        Given the fact that a participant is either a client or a server the discoveryServer_client_syncperiod parameter
        can have a context defined meaning.
    */
    mp_sync = new DServerEvent(this, TimeConv::Time_t2MilliSecondsDouble(m_discovery.discoveryServer_client_syncperiod));

    return true;
}

// TODO: MODIFY PDP READER TO RECEIVE DATA(P) MESSAGES FROM UNKNOWN CLIENTS
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

    mp_listener = new PDPListener(this);

    if (mp_RTPSParticipant->createReader(&mp_PDPReader, ratt, mp_PDPReaderHistory, mp_listener, c_EntityId_SPDPReader, true, false))
    {
        //#if HAVE_SECURITY
        //        mp_RTPSParticipant->set_endpoint_rtps_protection_supports(rout, false);
        //#endif
                // Initial peer list doesn't make sense in server scenario. Client should match its server list
        for (auto it = mp_builtin->m_DiscoveryServers.begin(); it != mp_builtin->m_DiscoveryServers.end(); ++it)
        {
            RemoteWriterAttributes rwatt;

            rwatt.guid = it->GetPDPWriter();
            rwatt.endpoint.multicastLocatorList.push_back(it->metatrafficMulticastLocatorList);
            rwatt.endpoint.unicastLocatorList.push_back(it->metatrafficUnicastLocatorList);
            rwatt.endpoint.topicKind = WITH_KEY;
            rwatt.endpoint.durabilityKind = TRANSIENT; // Server Information must be persistent
            rwatt.endpoint.reliabilityKind = RELIABLE;

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
        //#if HAVE_SECURITY
        //        mp_RTPSParticipant->set_endpoint_rtps_protection_supports(wout, false);
        //#endif
        for (auto it = mp_builtin->m_DiscoveryServers.begin(); it != mp_builtin->m_DiscoveryServers.end(); ++it)
        {
            RemoteReaderAttributes rratt;

            rratt.guid = it->GetPDPReader();
            rratt.endpoint.multicastLocatorList.push_back(it->metatrafficMulticastLocatorList);
            rratt.endpoint.unicastLocatorList.push_back(it->metatrafficUnicastLocatorList);
            rratt.endpoint.topicKind = WITH_KEY;
            rratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
            rratt.endpoint.reliabilityKind = RELIABLE;

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

    //#if HAVE_SECURITY
    //    if (getRTPSParticipant()->getAttributes().builtin.m_simpleEDP.enable_builtin_secure_publications_writer_and_subscriptions_reader)
    //    {
    //        participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_PUBLICATION_SECURE_ANNOUNCER;
    //        participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_SECURE_DETECTOR;
    //    }
    //
    //    if (getRTPSParticipant()->getAttributes().builtin.m_simpleEDP.enable_builtin_secure_subscriptions_writer_and_publications_reader)
    //    {
    //        participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_SECURE_ANNOUNCER;
    //        participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_PUBLICATION_SECURE_DETECTOR;
    //    }
    //#endif

}

// reviewed boundary




void PDPServer::assignRemoteEndpoints(ParticipantProxyData* pdata)
{
    // Verify if this participant is a server
    for (auto svr : mp_builtin->m_DiscoveryServers)
    {
        if (svr.guidPrefix == pdata->m_guid.guidPrefix)
        {
            svr.proxy = pdata;
        }
    }
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

    if (is_server)
    {
        // We should unmatch and match the PDP endpoints to renew the PDP reader and writer associated proxies
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
            watt.endpoint.durabilityKind = TRANSIENT_LOCAL;
            watt.endpoint.topicKind = WITH_KEY;

            mp_PDPReader->matched_writer_remove(watt);
            mp_PDPReader->matched_writer_add(watt);

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
            mp_PDPWriter->matched_reader_add(ratt);
        }
    }
}

bool PDPServer::all_servers_acknowledge_PDP()
{
    // check if already initialized
    assert(mp_PDPWriterHistory && mp_PDPWriter);

    // get a reference to client proxy data
    CacheChange_t * pPD;
    if (mp_PDPWriterHistory->get_min_change(&pPD))
    {
       return mp_PDPWriter->is_acked_by_all(pPD);
    }
    else
    {
        logError(RTPS_PDP, "ParticipantProxy data should have been added to client PDP history cache by a previous call to announceParticipantState()");
    }
  
    return false;
}

bool PDPServer::is_all_servers_PDPdata_updated()
{
    StatefulReader * pR = dynamic_cast<StatefulReader *>(mp_PDPReader);
    assert(pR);
    return pR->isInCleanState();
}

void PDPServer::announceParticipantState(bool new_change, bool dispose)
{
    PDP::announceParticipantState(new_change, dispose);

    if (!(dispose || new_change))
    {
        StatefulWriter * pW = dynamic_cast<StatefulWriter *>(mp_PDPWriter);
        assert(pW);
        pW->send_any_unacknowledge_changes();
    }
}

void PDPServer::match_all_server_EDP_endpoints()
{
    // PDP must have been initialize
    assert(mp_EDP);

    for (auto svr : mp_builtin->m_DiscoveryServers)
    {
        // We should have received the discovery info
        assert(svr.proxy);

        if (svr.proxy && !mp_EDP->areRemoteEndpointsMatched(svr.proxy))
        {
            mp_EDP->assignRemoteEndpoints(*svr.proxy);
        }
    }
}

} /* namespace rtps */
} /* namespace fastrtps */
} /* namespace eprosima */
