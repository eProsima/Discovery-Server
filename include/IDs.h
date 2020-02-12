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


#ifndef _IDS_H_
#define _IDS_H_

#include <string>

namespace eprosima {
namespace discovery_server {

// specific discovery server schema string literals:
static const std::string s_sDS("DS");
static const std::string s_sServers("servers");
static const std::string s_sServer("server");
static const std::string s_sClients("clients");
static const std::string s_sClient("client");
static const std::string s_sSimples("simples");
static const std::string s_sSimple("simple");
static const std::string s_sPersist("persist");
static const std::string s_sLP("ListeningPorts");
static const std::string s_sSL("ServersList");
static const std::string s_sRServer("RServer");
static const std::string s_sTime("time");
static const std::string s_sSomeone("someone");
static const std::string s_sCreationTime("creation_time");
static const std::string s_sRemovalTime("removal_time");
static const std::string s_sSnapshot("snapshot");
static const std::string s_sSnapshots("snapshots");
static const std::string s_sFile("file");
static const std::string s_sUserShutdown("user_shutdown");
static const std::string s_sPrefixValidation("prefix_validation");
static const std::string s_sListeningPort("listening_port");

// specific Snapshot schema string literals
static const std::string s_sDS_Snapshots("DS_Snapshots");
static const std::string s_sDS_Snapshot("DS_Snapshot");
static const std::string s_sTimestamp("timestamp");
static const std::string s_sProcessTime("process_time");
static const std::string s_sLastPdpCallback("last_pdp_callback_time");
static const std::string s_sLastEdpCallback("last_edp_callback_time");
static const std::string s_sDescription("description");
static const std::string s_sPtDB("ptdb");
static const std::string s_sPtDI("ptdi");
static const std::string s_sPublisher("publisher");
static const std::string s_sSubscriber("subscriber");
static const std::string s_sGUID_prefix("guid_prefix");
static const std::string s_sGUID_entity("guid_entity");
static const std::string s_sAlive("alive");
static const std::string s_sName("name");
static const std::string s_sTopic("topic");
static const std::string s_sType("type");

} // fastrtps
} // discovery_server

#endif // _IDS_H_
