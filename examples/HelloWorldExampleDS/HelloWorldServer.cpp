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
 * @file HelloWorldServer.cpp
 *
 */

#include "HelloWorldServer.h"
#include <fastrtps/participant/Participant.h>
#include <fastrtps/attributes/ParticipantAttributes.h>
#include <fastrtps/transport/TCPv4TransportDescriptor.h>

#include <fastrtps/Domain.h>
#include <fastrtps/utils/eClock.h>
#include <fastrtps/utils/IPLocator.h>

using namespace eprosima::fastrtps;
using namespace eprosima::fastrtps::rtps;

HelloWorldServer::HelloWorldServer():mp_participant(nullptr)
{
}

bool HelloWorldServer::init(bool tcp)
{
    ParticipantAttributes PParam;
    PParam.rtps.builtin.discovery_config.discoveryProtocol = DiscoveryProtocol_t::SERVER;
    PParam.rtps.ReadguidPrefix("4D.49.47.55.45.4c.5f.42.41.52.52.4f");
    PParam.rtps.builtin.domainId = 0;
    PParam.rtps.builtin.discovery_config.leaseDuration = c_TimeInfinite;
    PParam.rtps.setName("Participant_server");

    if (tcp)
    {											
        Locator_t server_address; // {kind=4 port=4273930240 address=0x0000024cd53398a8 "" }
        server_address.kind = LOCATOR_KIND_TCPv4;
        IPLocator::setLogicalPort(server_address, 65215);
        // IPLocator::setPhysicalPort(server_address, 9843); // redundant is already in the transport descriptor
        IPLocator::setIPv4(server_address, 127, 0, 0, 1);

        PParam.rtps.builtin.metatrafficUnicastLocatorList.push_back(server_address);

        std::shared_ptr<TCPv4TransportDescriptor> descriptor = std::make_shared<TCPv4TransportDescriptor>();
        descriptor->wait_for_tcp_negotiation = false;
        descriptor->add_listener_port(9843);

        PParam.rtps.useBuiltinTransports = false;
        PParam.rtps.userTransports.push_back(descriptor);
    }
    else
    {
        Locator_t server_address(LOCATOR_KIND_UDPv4, 65215);
        IPLocator::setIPv4(server_address, 127, 0, 0, 1);

        PParam.rtps.builtin.metatrafficUnicastLocatorList.push_back(server_address);
    }


    mp_participant = Domain::createParticipant(PParam);
    if(mp_participant==nullptr)
        return false;

    return true;
}

HelloWorldServer::~HelloWorldServer() {
    // TODO Auto-generated destructor stub
    Domain::removeParticipant(mp_participant);
}


void HelloWorldServer::run()
{
    std::cout << "Server running. Please press enter to stop the server" << std::endl;
    std::cin.ignore();
}

