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

#include "LateJoiner.h"

#include <fstream>

#include "log/DSLog.h"
#include "DiscoveryServerManager.h"

using namespace eprosima::fastdds;
using namespace eprosima::fastdds::dds;
using namespace eprosima::discovery_server;

// delayed creation of a new participant
void DelayedParticipantCreation::operator ()(
        DiscoveryServerManager& manager ) /*override*/
{

    DomainParticipant* p = DomainParticipantFactory::get_instance()->create_participant(42, qos, &manager);

    Publisher* publisher_ = p->create_publisher(PUBLISHER_QOS_DEFAULT);
    Subscriber* subscriber_ = p->create_subscriber(SUBSCRIBER_QOS_DEFAULT);

    ParticipantCreatedEntityInfo info;

    info.participant = p;
    info.publisher = publisher_;
    info.subscriber = subscriber_;

    if (p)
    {
        (manager.*participant_creation_function)(p); // addServer or addClient
        participant_guid = p->guid();
        // update the associated DelayedParticipantDestruction
        if (removal_event)
        {
            removal_event->SetGuid(participant_guid);
        }

        manager.setParticipantInfo(participant_guid, info);

        LOG_INFO("New participant called " << qos.name() << " with prefix " << p->guid());
    }
    else
    {
        LOG_ERROR("DiscoveryServerManager couldn't create the participant " << qos.wire_protocol().prefix);
    }
}

// delayed destruction of a new participant
void DelayedParticipantDestruction::operator ()(
        DiscoveryServerManager& manager) /*override*/
{
    DomainParticipant* p = manager.removeParticipant(participant_id);
    if (p)
    {
        std::string name = p->get_qos().name().to_string();

        ReturnCode_t ret = manager.deleteParticipant(p);
        if (RETCODE_OK != ret)
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

const std::string LateJoinerDataTraits<DataWriter>::endpoint_type("DataWriter");
const LateJoinerDataTraits<DataWriter>::AddEndpoint LateJoinerDataTraits<DataWriter>::add_endpoint_function =
        &DiscoveryServerManager::addDataWriter;
const LateJoinerDataTraits<DataWriter>::GetEndpoint LateJoinerDataTraits<DataWriter>::retrieve_endpoint_function =
        &DiscoveryServerManager::removePublisher;
const LateJoinerDataTraits<DataWriter>::removeEndpoint LateJoinerDataTraits<DataWriter>::remove_endpoint_function =
        &DiscoveryServerManager::deleteDataWriter;

/*static*/
DataWriter* LateJoinerDataTraits<DataWriter>::createEndpoint(
        DomainEntity* publisher,
        Topic* topic,
        const std::string& profile_name,
        void*)
{

    Publisher* pub = static_cast<Publisher*>(publisher);

    if (profile_name.empty())
    {
        return pub->create_datawriter(topic, DATAWRITER_QOS_DEFAULT);
    }
    else
    {
        return pub->create_datawriter_with_profile(topic, profile_name);
    }
}

const std::string LateJoinerDataTraits<DataReader>::endpoint_type("DataReader");
const LateJoinerDataTraits<DataReader>::AddEndpoint LateJoinerDataTraits<DataReader>::add_endpoint_function =
        &DiscoveryServerManager::addDataReader;
const LateJoinerDataTraits<DataReader>::GetEndpoint LateJoinerDataTraits<DataReader>::retrieve_endpoint_function =
        &DiscoveryServerManager::removeSubscriber;
const LateJoinerDataTraits<DataReader>::removeEndpoint LateJoinerDataTraits<DataReader>::remove_endpoint_function =
        &DiscoveryServerManager::deleteDataReader;

/*static*/
DataReader* LateJoinerDataTraits<DataReader>::createEndpoint(
        DomainEntity* subscriber,
        Topic* topic,
        const std::string& profile_name,
        SubscriberListener* list /* = nullptr*/)
{
    Subscriber* sub = static_cast<Subscriber*>(subscriber);

    if (profile_name.empty())
    {
        return sub->create_datareader(topic, DATAREADER_QOS_DEFAULT);
    }
    else
    {
        return sub->create_datareader_with_profile(topic, profile_name, list);
    }

}

void DelayedSnapshot::operator ()(
        DiscoveryServerManager& manager)  /*override*/
{
    manager.takeSnapshot(std::chrono::steady_clock::now(), description, if_someone, show_liveliness_);
}

void DelayedEnvironmentModification::operator ()(
        DiscoveryServerManager& man)  /*override*/
{
    (void)man;

    char* data;
    data = getenv("FASTDDS_ENVIRONMENT_FILE");
    if (nullptr != data)
    {
        std::ofstream output_file(data);
        output_file << "{" << std::endl;
        output_file << "\t\"" << attributes.first << "\" :\"" << attributes.second << "\"" << std::endl;
        output_file << "}" << std::endl;
    }
    else
    {
        LOG_ERROR("Empty FASTDDS_ENVIRONMENT_FILE variable");
        return;
    }
}
