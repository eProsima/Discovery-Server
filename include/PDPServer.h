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
 * @file PDPServer.h
 *
 */

#ifndef PDPSERVER_H_
#define PDPSERVER_H_
#ifndef DOXYGEN_SHOULD_SKIP_THIS_PUBLIC

#include <fastrtps\rtps\builtin\discovery\participant\PDP.h>
#include "DServerEvent.h"

#include <list>

namespace eprosima {
namespace fastrtps{
namespace rtps {

class StatefulWriter;
class StatefulReader;

/**
 * Class PDPServer manages server side of the discovery server mechanism
 *@ingroup DISCOVERY_MODULE
 */
class PDPServer : public PDP
{
    friend class DServerEvent;
    friend class PDPServerListener;

    typedef std::list<const ParticipantProxyData *> pending_matches_list;
    typedef std::set<InstanceHandle_t> key_list;

    //! EDP pending matches
    pending_matches_list _p2match;

    //! Keys to wipe out from WriterHistory because its related Participants have been removed
    key_list _demises;

    public:
    /**
     * Constructor
     * @param builtin Pointer to the BuiltinProcols object.
     */
    PDPServer(BuiltinProtocols* builtin);
    ~PDPServer();

    void initializeParticipantProxyData(ParticipantProxyData* participant_data) override;

    /**
     * Initialize the PDP.
     * @param part Pointer to the RTPSParticipant.
     * @return True on success
     */
    bool initPDP(RTPSParticipantImpl* part) override;

    /**
     * Create the SPDP Writer and Reader
     * @return True if correct.
     */
    bool createPDPEndpoints() override;

    /**
     * Methods to update WriterHistory with reader information
     */

    //! Callback to remove unnecesary WriterHistory info
    void trimWriterHistory();

    /**
     * Add participant CacheChange_ts from reader to writer
     * @return True if successfully modified WriterHistory
     */
    bool addParticipantToHistory(const CacheChange_t &);

    /**
     * Trigger the participant CacheChange_t removal system  
     * @return True if successfully modified WriterHistory
     */
    void removeParticipantFromHistory(const InstanceHandle_t &);

    /**
     * Check if all servers have acknowledge the client PDP data
     * @return True if all can reach the client
     */
    bool all_servers_acknowledge_PDP();

    /**
     * Check if we have our PDP received data updated
     * @return True if we known all the participants the servers are aware of
     */
    bool is_all_servers_PDPdata_updated();

    /**
     * These methods wouldn't be needed under perfect server operation (no need of dynamic endpoint allocation) but must be implemented
     * to solve server shutdown situations.
     * @param pdata Pointer to the RTPSParticipantProxyData object.
     */
    void assignRemoteEndpoints(ParticipantProxyData* pdata) override;
    void removeRemoteEndpoints(ParticipantProxyData * pdata) override;

    //!Matching server EDP endpoints
    void match_all_server_EDP_endpoints();

    private:

    /**
    * TimedEvent for server synchronization: 
    *   first stage: periodically resend the local RTPSParticipant information until all servers have acknowledge reception
    *   second stage: waiting PDP info is up to date before allowing EDP matching
    */ 
    DServerEvent * mp_sync;
};

}
} /* namespace rtps */
} /* namespace eprosima */
#endif
#endif /* PDPSERVER_H_ */
