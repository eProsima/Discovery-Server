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
 * @file HelloWorldSubscriber.cpp
 *
 */

#include "HelloWorldSubscriber.h"

#include <random>
#include <chrono>
#include <thread>
#include <fastdds/rtps/transport/TCPv4TransportDescriptor.h>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/qos/SubscriberQos.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>

#include <fastrtps/utils/IPLocator.h>

using namespace eprosima::fastdds;
using namespace eprosima::fastrtps::rtps;
using namespace eprosima::fastdds::rtps;
using namespace eprosima::fastdds::dds;

HelloWorldSubscriber::HelloWorldSubscriber()
    : mp_participant(nullptr)
    , mp_subscriber(nullptr)
    , mp_reader(nullptr)
{
}

bool HelloWorldSubscriber::init(
        Locator server_address)
{

    LocatorList_t remote_server;

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

        remote_server.push_back(server_address);
        participant_qos.wire_protocol().builtin.discovery_config.m_DiscoveryServers.push_back(remote_server);
        participant_qos.transport().use_builtin_transports = false;
        std::shared_ptr<TCPv4TransportDescriptor> descriptor = std::make_shared<TCPv4TransportDescriptor>();

        // Generate a listening port for the client
        std::default_random_engine gen(std::chrono::system_clock::now().time_since_epoch().count());
        std::uniform_int_distribution<int> rdn(49152, 57343);
        descriptor->add_listener_port(rdn(gen)); // IANA ephemeral port number
        descriptor->wait_for_tcp_negotiation = false;
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

        remote_server.push_back(server_address);
        participant_qos.wire_protocol().builtin.discovery_config.m_DiscoveryServers.push_back(remote_server);
    }

    participant_qos.wire_protocol().builtin.discovery_config.discoveryProtocol = DiscoveryProtocol_t::CLIENT;
    participant_qos.wire_protocol().participant_id = 3;
    participant_qos.wire_protocol().builtin.discovery_config.leaseDuration = eprosima::fastrtps::c_TimeInfinite;
    participant_qos.name("Participant_sub");

    mp_participant = DomainParticipantFactory::get_instance()->create_participant(0, participant_qos);

    if (mp_participant == nullptr)
    {
        return false;
    }

    TypeSupport ts(new HelloWorldPubSubType());

    mp_participant->register_type(ts);

    TopicQos topic_qos = TOPIC_QOS_DEFAULT;

    topic_qos.history().depth = 30;
    topic_qos.history().kind = KEEP_LAST_HISTORY_QOS;
    topic_qos.resource_limits().max_samples = 50;
    topic_qos.resource_limits().allocated_samples = 20;

    SubscriberQos subscriber_qos = SUBSCRIBER_QOS_DEFAULT;
    DataReaderQos datareader_qos = DATAREADER_QOS_DEFAULT;

    datareader_qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    datareader_qos.durability().kind = TRANSIENT_LOCAL_DURABILITY_QOS;

    mp_subscriber = mp_participant->create_subscriber(subscriber_qos);

    Topic* topic = mp_participant->create_topic("HelloWorldTopic", ts.get_type_name(), topic_qos);

    mp_reader = mp_subscriber->create_datareader(topic, datareader_qos, &m_listener);

    if (mp_reader == nullptr)
    {
        return false;
    }

    return true;
}

HelloWorldSubscriber::~HelloWorldSubscriber()
{
    mp_participant->delete_contained_entities();
    DomainParticipantFactory::get_instance()->delete_participant(mp_participant);
}

void HelloWorldSubscriber::SubListener::on_subscription_matched(
        DataReader* /*sub*/,
        const SubscriptionMatchedStatus& info)
{
    if (info.current_count_change == 1)
    {
        n_matched = info.current_count;
        std::cout << "Subscriber matched" << std::endl;
    }
    else if (info.current_count_change == -1)
    {
        n_matched = info.current_count;
        std::cout << "Subscriber unmatched" << std::endl;
    }
}

void HelloWorldSubscriber::SubListener::on_data_available(
        DataReader* sub)
{
    if (sub->take_next_sample((void*)&m_hello, &m_info) == ReturnCode_t::RETCODE_OK)
    {
        if (m_info.sample_state == NOT_READ_SAMPLE_STATE)
        {
            this->n_samples++;
            // Print your structure data here.
            std::cout << "Message " << m_hello.message() << " " << m_hello.index() << " RECEIVED" << std::endl;
        }
    }

}

void HelloWorldSubscriber::run()
{
    std::cout << "Subscriber running. Please press enter to stop the Subscriber" << std::endl;
    std::cin.ignore();
}

void HelloWorldSubscriber::run(
        uint32_t number)
{
    std::cout << "Subscriber running until " << number << "samples have been received" << std::endl;
    while (number > this->m_listener.n_samples)
    {
        std::this_thread::sleep_for(std::chrono::duration<uint32_t, std::milli>(500));
    }
}
