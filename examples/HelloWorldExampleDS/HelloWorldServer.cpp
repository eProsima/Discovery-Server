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

#include <sstream>

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/rtps/transport/TCPv4TransportDescriptor.h>
#include <fastdds/rtps/transport/TCPv6TransportDescriptor.h>

#include <fastrtps/utils/IPLocator.h>

using namespace eprosima::fastrtps;
using namespace eprosima::fastrtps::rtps;

using namespace eprosima::fastdds;
using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;

HelloWorldServer::HelloWorldServer()
    : mp_participant(nullptr)
{
}

bool HelloWorldServer::init(
        Locator server_address)
{

    eprosima::fastdds::dds::DomainParticipantQos participant_qos = eprosima::fastdds::dds::PARTICIPANT_QOS_DEFAULT;

    participant_qos.wire_protocol().builtin.discovery_config.discoveryProtocol = DiscoveryProtocol_t::SERVER;
    std::istringstream iss("44.49.53.43.53.45.52.56.45.52.5F.31");
    iss >> participant_qos.wire_protocol().prefix;
    participant_qos.wire_protocol().builtin.discovery_config.leaseDuration = c_TimeInfinite;
    participant_qos.wire_protocol().builtin.discovery_config.initial_announcements.count = 0;
    participant_qos.name("Participant_server");

    uint16_t default_port = IPLocator::getPhysicalPort(server_address.port);

    // The library is wise enough to handle the empty IP address scenario by replacing it with
    // all local interfaces

    if (server_address.kind == LOCATOR_KIND_TCPv4 ||
            server_address.kind == LOCATOR_KIND_TCPv6)
    {

        // logical port cannot be customize in this example
        IPLocator::setIPv4(server_address, 127, 0, 0, 1);
        IPLocator::setLogicalPort(server_address, 65215);
        IPLocator::setPhysicalPort(server_address, default_port); // redundant is already in the transport descriptor

        participant_qos.wire_protocol().builtin.metatrafficUnicastLocatorList.push_back(server_address);

        std::shared_ptr<TCPTransportDescriptor> descriptor;

        if (server_address.kind == LOCATOR_KIND_TCPv4)
        {
            descriptor = std::make_shared<TCPv4TransportDescriptor>();
        }
        else
        {
            descriptor = std::make_shared<TCPv6TransportDescriptor>();
        }

        descriptor->add_listener_port(default_port);
        participant_qos.transport().use_builtin_transports = false;
        participant_qos.transport().user_transports.push_back(descriptor);
    }
    else
    {
        server_address.port = default_port;
        IPLocator::setIPv4(server_address, 127, 0, 0, 1);
        participant_qos.wire_protocol().builtin.metatrafficUnicastLocatorList.push_back(server_address);
    }

    participant_qos.wire_protocol().participant_id = 1;
    mp_participant = DomainParticipantFactory::get_instance()->create_participant(0, participant_qos);

    if (mp_participant == nullptr)
    {
        return false;
    }

    return true;
}

HelloWorldServer::~HelloWorldServer()
{
    DomainParticipantFactory::get_instance()->delete_participant(mp_participant);
}

void HelloWorldServer::run()
{
    std::cout << "Server running. Please press enter to stop the server" << std::endl;
    std::cin.ignore();
}
