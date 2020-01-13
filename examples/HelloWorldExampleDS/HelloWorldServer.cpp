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
#include <fastrtps/transport/TCPv6TransportDescriptor.h>

#include <fastrtps/Domain.h>
#include <fastrtps/utils/IPLocator.h>
#include <fastrtps/utils/IPFinder.h>

using namespace eprosima::fastrtps;
using namespace eprosima::fastrtps::rtps;

HelloWorldServer::HelloWorldServer()
    : mp_participant(nullptr)
{
}

bool HelloWorldServer::init(Locator_t server_address)
{
    ParticipantAttributes PParam;
    PParam.rtps.builtin.discovery_config.discoveryProtocol = DiscoveryProtocol_t::SERVER;
    PParam.rtps.ReadguidPrefix("4D.49.47.55.45.4c.5f.42.41.52.52.4f");
    PParam.rtps.builtin.domainId = 0;
    PParam.rtps.builtin.discovery_config.leaseDuration = c_TimeInfinite;
    PParam.rtps.setName("Participant_server");

    // If no address provided get all local
    LocatorList_t local_interfaces;
    
    if(!IsAddressDefined(server_address))
    {
        std::vector<IPFinder::info_IP> loc_info;
        IPFinder::getIPs(&loc_info, false);

        for(const IPFinder::info_IP & ip : loc_info)
        {
            local_interfaces.push_back(ip.locator);
        }

    }
    else
    {
        local_interfaces.push_back(server_address);
    }

    uint16_t default_port = IPLocator::getPhysicalPort(server_address.port);

    if (server_address.kind == LOCATOR_KIND_TCPv4 ||
        server_address.kind == LOCATOR_KIND_TCPv6)
    {

        for(Locator_t & loc : local_interfaces)
        {
            if(IPLocator::hasIPv4(loc.kind) == IPLocator::hasIPv4(server_address.kind))
            {
                loc.kind = server_address.kind;

                // logical port cannot be customize in this example
                IPLocator::setLogicalPort(loc, 65215);
                IPLocator::setPhysicalPort(loc, default_port); // redundant is already in the transport descriptor

                PParam.rtps.builtin.metatrafficUnicastLocatorList.push_back(loc);
            }
        }

        std::shared_ptr<TCPTransportDescriptor> descriptor;

        if(server_address.kind == LOCATOR_KIND_TCPv4)
        {
            descriptor = std::make_shared<TCPv4TransportDescriptor>();
        }
        else
        {
            descriptor = std::make_shared<TCPv6TransportDescriptor>();
        }

        descriptor->wait_for_tcp_negotiation = false;
        descriptor->add_listener_port(default_port);
        PParam.rtps.useBuiltinTransports = false;
        PParam.rtps.userTransports.push_back(descriptor);
    }
    else
    {
        for(Locator_t & loc : local_interfaces)
        {
            if(loc.kind == server_address.kind)
            {
                loc.port = default_port;
                PParam.rtps.builtin.metatrafficUnicastLocatorList.push_back(loc);
            }
        }
    }

    mp_participant = Domain::createParticipant(PParam);
    if (mp_participant==nullptr)
        return false;

    return true;
}

HelloWorldServer::~HelloWorldServer() 
{
    Domain::removeParticipant(mp_participant);
}


void HelloWorldServer::run()
{
    std::cout << "Server running. Please press enter to stop the server" << std::endl;
    std::cin.ignore();
}

