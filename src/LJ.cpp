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

#include "log/DSLog.h"
#include "LJ.h"
#include "DSManager.h"

#include <fastrtps/transport/UDPv4TransportDescriptor.h>

using namespace eprosima::fastrtps;
using namespace eprosima::discovery_server;
// New API
using namespace eprosima::fastdds::dds;

// delayed creation of a new participant
void DelayedParticipantCreation::operator ()(
        DSManager& man ) /*override*/
{

    DomainParticipantQos default_qos_ = PARTICIPANT_QOS_DEFAULT;
    default_qos_.user_data().setValue(attributes.rtps.userData);
    default_qos_.allocation() = attributes.rtps.allocation;
    default_qos_.properties() = attributes.rtps.properties;
    default_qos_.wire_protocol().prefix = attributes.rtps.prefix;
    default_qos_.wire_protocol().participant_id = attributes.rtps.participantID;
    default_qos_.wire_protocol().builtin = attributes.rtps.builtin;
    default_qos_.wire_protocol().port = attributes.rtps.port;
    default_qos_.wire_protocol().throughput_controller = attributes.rtps.throughputController;
    default_qos_.wire_protocol().default_unicast_locator_list = attributes.rtps.defaultUnicastLocatorList;
    default_qos_.wire_protocol().default_multicast_locator_list = attributes.rtps.defaultMulticastLocatorList;
    default_qos_.transport().user_transports = attributes.rtps.userTransports;
    default_qos_.transport().use_builtin_transports = attributes.rtps.useBuiltinTransports;
    default_qos_.transport().send_socket_buffer_size = attributes.rtps.sendSocketBufferSize;
    default_qos_.transport().listen_socket_buffer_size = attributes.rtps.listenSocketBufferSize;
    default_qos_.name() = attributes.rtps.getName();
    default_qos_.flow_controllers() = attributes.rtps.flow_controllers;

    DomainParticipant* p = DomainParticipantFactory::get_instance()->create_participant(attributes.domainId,
                    default_qos_, &man);

    PublisherQos default_publisher_qos_ = PUBLISHER_QOS_DEFAULT;

    fastdds::dds::Publisher* publisher_ = p->create_publisher(default_publisher_qos_);

    SubscriberQos default_subscriber_qos_ = SUBSCRIBER_QOS_DEFAULT;

    fastdds::dds::Subscriber* subscriber_ = p->create_subscriber(default_subscriber_qos_);

    ParticipantCreatedEntityInfo info;

    info.participant = p;
    info.publisher = publisher_;
    info.subscriber = subscriber_;


    if (p)
    {
        (man.*participant_creation_function)(p); // addServer or addClient
        participant_guid = p->guid();
        // update the associated DelayedParticipantDestruction
        if (removal_event)
        {
            removal_event->SetGuid(participant_guid);
        }

        man.setParticipantInfo(participant_guid, info);

        LOG_INFO("New participant called " << attributes.rtps.getName() << " with prefix " << p->guid());
    }
    else
    {
        LOG_ERROR("DSManager couldn't create the participant " << attributes.rtps.prefix  );
    }
}

// delayed destruction of a new participant
void DelayedParticipantDestruction::operator ()(
        DSManager& man) /*override*/
{
    DomainParticipant* p = man.removeParticipant(participant_id);


    if (p)
    {
        std::string name = p->get_qos().name().to_string();

        ReturnCode_t ret = man.deleteParticipant(p);
        //DomainParticipantFactory::get_instance()->delete_participant(p);
        if (ReturnCode_t::RETCODE_OK != ret)
        {
            LOG_ERROR("Error during delayed deletion of Participant");
        }
        LOG_INFO("Removed participant called " << name << " with prefix " << participant_id );
    }
    else
    {
        LOG_ERROR("Destroying nonexisting participant");
    }
}

void DelayedParticipantDestruction::SetGuid(
        const GUID_t& id)
{
    if (id != GUID_t::unknown())
    {
        participant_id = id; // update
    }
}

// static LJD_atts pointer to member
const std::string LateJoinerDataTraits<DataWriter>::endpoint_type("Publisher");
const LateJoinerDataTraits<DataWriter>::AddEndpoint LateJoinerDataTraits<DataWriter>::add_endpoint_function =
        &DSManager::addPublisher;
const LateJoinerDataTraits<DataWriter>::GetEndpoint LateJoinerDataTraits<DataWriter>::retrieve_endpoint_function =
        &DSManager::removePublisher;
const LateJoinerDataTraits<DataWriter>::removeEndpoint LateJoinerDataTraits<DataWriter>::remove_endpoint_function =
        &DSManager::deletePublisher;

/*static*/
DataWriter* LateJoinerDataTraits<DataWriter>::createEndpoint(
        //    fastdds::dds::Publisher* part,
        fastdds::dds::DomainEntity* part,
        Topic* topic,
        const PublisherAttributes& pa,
        void*)
{
    fastdds::dds::Publisher* publisher = static_cast<fastdds::dds::Publisher*>(part);
    DataWriterQos default_qos_ = publisher->get_default_datawriter_qos();

    default_qos_.writer_resource_limits().matched_subscriber_allocation = pa.matched_subscriber_allocation;
    default_qos_.properties() = pa.properties;
    default_qos_.throughput_controller() = pa.throughputController;
    default_qos_.endpoint().unicast_locator_list = pa.unicastLocatorList;
    default_qos_.endpoint().multicast_locator_list = pa.multicastLocatorList;
    default_qos_.endpoint().remote_locator_list = pa.remoteLocatorList;
    default_qos_.endpoint().history_memory_policy = pa.historyMemoryPolicy;
    default_qos_.endpoint().user_defined_id = pa.getUserDefinedID();
    default_qos_.endpoint().entity_id = pa.getEntityID();
    default_qos_.reliable_writer_qos().times = pa.times;
    default_qos_.reliable_writer_qos().disable_positive_acks = pa.qos.m_disablePositiveACKs;
    default_qos_.durability() = pa.qos.m_durability;
    default_qos_.durability_service() = pa.qos.m_durabilityService;
    default_qos_.deadline() = pa.qos.m_deadline;
    default_qos_.latency_budget() = pa.qos.m_latencyBudget;
    default_qos_.liveliness() = pa.qos.m_liveliness;
    default_qos_.reliability() = pa.qos.m_reliability;
    default_qos_.lifespan() = pa.qos.m_lifespan;
    default_qos_.user_data().setValue(pa.qos.m_userData);
    default_qos_.ownership() = pa.qos.m_ownership;
    default_qos_.ownership_strength() = pa.qos.m_ownershipStrength;
    default_qos_.destination_order() = pa.qos.m_destinationOrder;
    default_qos_.representation() = pa.qos.representation;
    default_qos_.publish_mode() = pa.qos.m_publishMode;
    default_qos_.history() = pa.topic.historyQos;
    default_qos_.resource_limits() = pa.topic.resourceLimitsQos;
    default_qos_.data_sharing() = pa.qos.data_sharing;

    return publisher->create_datawriter(topic, default_qos_);
    //return Domain::createPublisher(part, pa);
}

const std::string LateJoinerDataTraits<DataReader>::endpoint_type("Subscriber");
const LateJoinerDataTraits<DataReader>::AddEndpoint LateJoinerDataTraits<DataReader>::add_endpoint_function =
        &DSManager::addSubscriber;
const LateJoinerDataTraits<DataReader>::GetEndpoint LateJoinerDataTraits<DataReader>::retrieve_endpoint_function =
        &DSManager::removeSubscriber;
const LateJoinerDataTraits<DataReader>::removeEndpoint LateJoinerDataTraits<DataReader>::remove_endpoint_function =
        &DSManager::deleteSubscriber;


/*static*/
DataReader* LateJoinerDataTraits<DataReader>::createEndpoint(
        //fastdds::dds::Subscriber* part,
        fastdds::dds::DomainEntity* part,
        Topic* topic,
        const SubscriberAttributes& sa,
        fastdds::dds::SubscriberListener* list /* = nullptr*/)
{
    fastdds::dds::Subscriber* subscriber = static_cast<fastdds::dds::Subscriber*>(part);
    DataReaderQos default_qos_ = subscriber->get_default_datareader_qos();

    default_qos_.reader_resource_limits().matched_publisher_allocation = sa.matched_publisher_allocation;
    default_qos_.properties() = sa.properties;
    default_qos_.expects_inline_qos(sa.expectsInlineQos);
    default_qos_.endpoint().unicast_locator_list = sa.unicastLocatorList;
    default_qos_.endpoint().multicast_locator_list = sa.multicastLocatorList;
    default_qos_.endpoint().remote_locator_list = sa.remoteLocatorList;
    default_qos_.endpoint().history_memory_policy = sa.historyMemoryPolicy;
    default_qos_.endpoint().user_defined_id = sa.getUserDefinedID();
    default_qos_.endpoint().entity_id = sa.getEntityID();
    default_qos_.reliable_reader_qos().times = sa.times;
    default_qos_.reliable_reader_qos().disable_positive_ACKs = sa.qos.m_disablePositiveACKs;
    default_qos_.durability() = sa.qos.m_durability;
    default_qos_.durability_service() = sa.qos.m_durabilityService;
    default_qos_.deadline() = sa.qos.m_deadline;
    default_qos_.latency_budget() = sa.qos.m_latencyBudget;
    default_qos_.liveliness() = sa.qos.m_liveliness;
    default_qos_.reliability() = sa.qos.m_reliability;
    default_qos_.lifespan() = sa.qos.m_lifespan;
    default_qos_.user_data().setValue(sa.qos.m_userData);
    default_qos_.ownership() = sa.qos.m_ownership;
    default_qos_.destination_order() = sa.qos.m_destinationOrder;
    default_qos_.type_consistency().type_consistency = sa.qos.type_consistency;
    default_qos_.type_consistency().representation = sa.qos.representation;
    default_qos_.time_based_filter() = sa.qos.m_timeBasedFilter;
    default_qos_.history() = sa.topic.historyQos;
    default_qos_.resource_limits() = sa.topic.resourceLimitsQos;
    default_qos_.data_sharing() = sa.qos.data_sharing;
    return subscriber->create_datareader(topic, default_qos_, list);

}

void DelayedSnapshot::operator ()(
        DSManager& man)  /*override*/
{
    man.takeSnapshot(std::chrono::steady_clock::now(), description, if_someone, show_liveliness_);
}
