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
 * @file HelloWorldPublisher.cpp
 *
 */

#include "HelloWorldPublisher.h"

#include <chrono>
#include <random>
#include <thread>

#include <fastdds/rtps/transport/TCPv4TransportDescriptor.h>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/qos/PublisherQos.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/utils/IPLocator.h>

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;

HelloWorldPublisher::HelloWorldPublisher()
    : mp_participant(nullptr)
    , mp_publisher(nullptr)
    , mp_writer(nullptr)
{
}

bool HelloWorldPublisher::init(
        Locator server_address)
{
    m_hello.index(0);
    m_hello.message("HelloWorld");

    RemoteServerAttributes ratt;

    ratt.ReadguidPrefix("44.49.53.43.53.45.52.56.45.52.5F.31");

    DomainParticipantQos participant_qos = PARTICIPANT_QOS_DEFAULT;

    uint16_t default_port = IPLocator::getPhysicalPort(server_address.port);

    if (server_address.kind == LOCATOR_KIND_TCPv4 ||
            server_address.kind == LOCATOR_KIND_TCPv6)
    {
        if (!IsAddressDefined(server_address))
        {
            server_address.kind = LOCATOR_KIND_TCPv4;
            IPLocator::setIPv4(server_address, 127, 0, 0, 1);
        }

        // server logical port is not customizable in this example
        IPLocator::setPhysicalPort(server_address, default_port);
        IPLocator::setLogicalPort(server_address, 65215);

        ratt.metatrafficUnicastLocatorList.push_back(server_address);
        participant_qos.wire_protocol().builtin.discovery_config.m_DiscoveryServers.push_back(ratt);
        participant_qos.transport().use_builtin_transports = false;
        std::shared_ptr<TCPv4TransportDescriptor> descriptor = std::make_shared<TCPv4TransportDescriptor>();

        // Generate a listening port for the client
        std::default_random_engine gen(std::chrono::system_clock::now().time_since_epoch().count());
        std::uniform_int_distribution<int> rdn(57344, 65535);
        descriptor->add_listener_port(rdn(gen)); // IANA ephemeral port number
        participant_qos.transport().user_transports.push_back(descriptor);
    }
    else
    {
        if (!IsAddressDefined(server_address))
        {
            server_address.kind = LOCATOR_KIND_UDPv4;
            server_address.port = default_port;
            IPLocator::setIPv4(server_address, 127, 0, 0, 1);
        }

        ratt.metatrafficUnicastLocatorList.push_back(server_address);
        participant_qos.wire_protocol().builtin.discovery_config.m_DiscoveryServers.push_back(ratt);
    }

    participant_qos.wire_protocol().builtin.discovery_config.discoveryProtocol = DiscoveryProtocol_t::CLIENT;
    participant_qos.wire_protocol().participant_id = 2;
    participant_qos.wire_protocol().builtin.discovery_config.leaseDuration = eprosima::fastdds::c_TimeInfinite;
    participant_qos.name("Participant_pub");

    mp_participant = DomainParticipantFactory::get_instance()->create_participant(0, participant_qos);

    if (mp_participant == nullptr)
    {
        return false;
    }

    TypeSupport ts(new HelloWorldPubSubType());

    mp_participant->register_type(ts);

    TopicQos topic_qos = TOPIC_QOS_DEFAULT;

    topic_qos.history().depth = 30;
    topic_qos.history().kind = eprosima::fastdds::KEEP_LAST_HISTORY_QOS;
    topic_qos.resource_limits().max_samples = 50;
    topic_qos.resource_limits().allocated_samples = 20;

    PublisherQos publisher_qos = PUBLISHER_QOS_DEFAULT;
    DataWriterQos datawriter_qos = DATAWRITER_QOS_DEFAULT;

    datawriter_qos.reliable_writer_qos().times.heartbeatPeriod.seconds = 2;
    datawriter_qos.reliable_writer_qos().times.heartbeatPeriod.nanosec = 0;
    datawriter_qos.reliability().kind = eprosima::fastdds::RELIABLE_RELIABILITY_QOS;

    mp_publisher = mp_participant->create_publisher(publisher_qos);

    Topic* topic = mp_participant->create_topic("HelloWorldTopic", ts.get_type_name(), topic_qos);

    mp_writer = mp_publisher->create_datawriter(topic, datawriter_qos, &m_listener);

    if (mp_writer == nullptr)
    {
        return false;
    }

    return true;

}

HelloWorldPublisher::~HelloWorldPublisher()
{
    mp_participant->delete_contained_entities();
    DomainParticipantFactory::get_instance()->delete_participant(mp_participant);
}

void HelloWorldPublisher::PubListener::on_publication_matched(
        DataWriter* /*pub*/,
        const PublicationMatchedStatus& info)
{
    if (info.current_count_change == 1)
    {
        n_matched = info.current_count;
        std::cout << "DataWriter matched." << std::endl;
    }
    else if (info.current_count_change == -1)
    {
        n_matched = info.current_count;
        std::cout << "DataWriter unmatched." << std::endl;
    }
}

void HelloWorldPublisher::runThread(
        uint32_t samples,
        uint32_t sleep)
{
    if (samples == 0)
    {
        while (!stop)
        {
            if (publish(false))
            {
                std::cout << "Message: " << m_hello.message() << " with index: " << m_hello.index() << " SENT" <<
                    std::endl;
            }
            std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(sleep));
        }
    }
    else
    {
        for (uint32_t i = 0; i < samples; ++i)
        {
            if (!publish())
            {
                --i;
            }
            else
            {
                std::cout << "Message: " << m_hello.message() << " with index: " << m_hello.index() << " SENT" <<
                    std::endl;
            }
            std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(sleep));
        }
    }
}

void HelloWorldPublisher::run(
        uint32_t samples,
        uint32_t sleep)
{
    stop = false;
    std::thread thread(&HelloWorldPublisher::runThread, this, samples, sleep);
    if (samples == 0)
    {
        std::cout << "Publisher running. Please press enter to stop the Publisher at any time." << std::endl;
        std::cin.ignore();
        stop = true;
    }
    else
    {
        std::cout << "Publisher running " << samples << " samples." << std::endl;
    }
    thread.join();
}

bool HelloWorldPublisher::publish(
        bool waitForListener)
{
    if (!waitForListener || m_listener.n_matched > 0)
    {
        m_hello.index(m_hello.index() + 1);
        mp_writer->write((void*)&m_hello);
        return true;
    }
    return false;
}
