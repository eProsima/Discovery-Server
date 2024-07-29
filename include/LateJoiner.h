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


#ifndef _LJ_H_
#define _LJ_H_

#include <string>
#include <thread>

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/rtps/common/Guid.hpp>

#include "DiscoveryServerManager.h"
#include "log/DSLog.h"
#include "../resources/xtypes/HelloWorldPubSubTypes.hpp"

namespace eprosima {
namespace discovery_server {

using eprosima::fastdds::rtps::GUID_t;

class DelayedParticipantDestruction;

class LateJoinerData // Late Joiner Data basic class
{
    // When the late joiner should be added
    std::chrono::steady_clock::time_point time;

public:

    LateJoinerData(
            const std::chrono::steady_clock::time_point& tp)
        : time(tp)
    {
    }

    virtual ~LateJoinerData()
    {
    }

    LateJoinerData() = delete;
    LateJoinerData(
            const LateJoinerData&) = default;
    LateJoinerData(
            LateJoinerData&&) = default;
    LateJoinerData& operator =(
            const LateJoinerData&) = default;
    LateJoinerData& operator =(
            LateJoinerData&&) = default;

    // Which operation should I do
    virtual void operator ()(
            DiscoveryServerManager& ) = 0;

    bool operator <(
            const LateJoinerData& event) const
    {
        return time < event.time;
    }

    void Wait() const
    {
        std::this_thread::sleep_until(time);
    }

    // return associated time_point
    std::chrono::steady_clock::time_point executionTime() const
    {
        return time;
    }

};

class DelayedParticipantCreation
    : public LateJoinerData // Delayed Participant Creation
{
    typedef void (DiscoveryServerManager::* AddParticipant)(
            fastdds::dds::DomainParticipant*);
    DomainParticipantQos qos;
    AddParticipant participant_creation_function;
    DelayedParticipantDestruction* removal_event;

public:

    GUID_t participant_guid; // participant GUID_t

    DelayedParticipantCreation(
            const std::chrono::steady_clock::time_point tp,
            DomainParticipantQos&& qos,
            AddParticipant m,
            DelayedParticipantDestruction* pD = nullptr)
        : LateJoinerData(tp)
        , qos(std::move(qos))
        , participant_creation_function(m)
        , removal_event(pD)
    {
    }

    ~DelayedParticipantCreation() override
    {
    }

    DelayedParticipantCreation() = delete;
    DelayedParticipantCreation(
            const DelayedParticipantCreation&) = default;
    DelayedParticipantCreation(
            DelayedParticipantCreation&&) = default;
    DelayedParticipantCreation& operator =(
            const DelayedParticipantCreation&) = default;
    DelayedParticipantCreation& operator =(
            DelayedParticipantCreation&&) = default;

    void operator ()(
            DiscoveryServerManager& ) override;
};

class DelayedParticipantDestruction
    : public LateJoinerData // Delayed Participant Destruction
{
    GUID_t participant_id; // participant to remove

public:

    DelayedParticipantDestruction(
            const std::chrono::steady_clock::time_point tp,
            GUID_t& id)
        : LateJoinerData(tp)
        , participant_id(id)
    {
    }

    ~DelayedParticipantDestruction() override
    {
    }

    DelayedParticipantDestruction() = delete;
    DelayedParticipantDestruction(
            const DelayedParticipantDestruction&) = default;
    DelayedParticipantDestruction(
            DelayedParticipantDestruction&&) = default;
    DelayedParticipantDestruction& operator =(
            const DelayedParticipantDestruction&) = default;
    DelayedParticipantDestruction& operator =(
            DelayedParticipantDestruction&&) = default;

    void operator ()(
            DiscoveryServerManager&) override;
    void SetGuid(
            const GUID_t&);

};

template<class ReaderWriter> struct LateJoinerDataTraits
{
};

template<> struct LateJoinerDataTraits<DataWriter>
{
    typedef void (DiscoveryServerManager::* AddEndpoint)(
            DataWriter*);
    typedef DataWriter* (DiscoveryServerManager::* GetEndpoint)(
            GUID_t&);
    typedef ReturnCode_t (DiscoveryServerManager::* removeEndpoint)(
            DataWriter*);

    static const std::string endpoint_type;
    static const AddEndpoint add_endpoint_function;
    static const GetEndpoint retrieve_endpoint_function;
    static const removeEndpoint remove_endpoint_function;

    // we don't want yet to listen on publisher callbacks
    static DataWriter* createEndpoint(
            fastdds::dds::DomainEntity* publisher,
            fastdds::dds::Topic* topic,
            const std::string& profile_name,
            void* = nullptr);
};

template<> struct LateJoinerDataTraits<DataReader>
{
    typedef void (DiscoveryServerManager::* AddEndpoint)(
            DataReader*);
    typedef DataReader* (DiscoveryServerManager::* GetEndpoint)(
            GUID_t&);
    typedef ReturnCode_t (DiscoveryServerManager::* removeEndpoint)(
            DataReader*);

    static const std::string endpoint_type;
    static const AddEndpoint add_endpoint_function;
    static const GetEndpoint retrieve_endpoint_function;
    static const removeEndpoint remove_endpoint_function;

    static DataReader* createEndpoint(
            fastdds::dds::DomainEntity* subscriber,
            fastdds::dds::Topic* topic,
            const std::string& profile_name,
            fastdds::dds::SubscriberListener* = nullptr);
};

template<class ReaderWriter> class DelayedEndpointDestruction;

template<class ReaderWriter>
class DelayedEndpointCreation
    : public LateJoinerData // Delayed Endpoint Creation
{
    std::string topic_name;
    std::string type_name;
    std::string topic_profile_name;
    std::string endpoint_profile_name;
    GUID_t participant_guid;
    DelayedEndpointDestruction<ReaderWriter>* linked_destruction_event;
    DelayedParticipantCreation* owner_event;  // associated participant event

public:

    DelayedEndpointCreation(
            const std::chrono::steady_clock::time_point tp,
            const std::string& topic_name,
            const std::string& type_name,
            const std::string& topic_profile_name,
            const std::string& endpoint_profile_name,
            GUID_t& pid,
            DelayedEndpointDestruction<ReaderWriter>* p = nullptr,
            DelayedParticipantCreation* part = nullptr)
        : LateJoinerData(tp)
        , topic_name(topic_name)
        , type_name(type_name)
        , topic_profile_name(topic_profile_name)
        , endpoint_profile_name(endpoint_profile_name)
        , participant_guid(pid)
        , linked_destruction_event(p)
        , owner_event(part)
    {
    }

    DelayedEndpointCreation(
            DelayedEndpointCreation&&);
    ~DelayedEndpointCreation() override;
    DelayedEndpointCreation& operator =(
            DelayedEndpointCreation&& d);

    DelayedEndpointCreation() = delete;
    DelayedEndpointCreation(
            const DelayedEndpointCreation&) = default;
    DelayedEndpointCreation& operator =(
            const DelayedEndpointCreation&) = default;

    void operator ()(
            DiscoveryServerManager&) override;

};

template<class ReaderWriter>
class DelayedEndpointDestruction
    : public LateJoinerData // Delayed Endpoint Destruction
{
    GUID_t endpoint_guid; // endpoint to remove

public:

    DelayedEndpointDestruction(
            const std::chrono::steady_clock::time_point& tp,
            GUID_t id = GUID_t::unknown())
        : LateJoinerData(tp)
        , endpoint_guid(id)
    {
    }

    ~DelayedEndpointDestruction() override
    {
    }

    DelayedEndpointDestruction() = delete;
    DelayedEndpointDestruction(
            const DelayedEndpointDestruction&) = default;
    DelayedEndpointDestruction(
            DelayedEndpointDestruction&&) = default;
    DelayedEndpointDestruction& operator =(
            const DelayedEndpointDestruction&) = default;
    DelayedEndpointDestruction& operator =(
            DelayedEndpointDestruction&&) = default;

    void operator ()(
            DiscoveryServerManager&) override;
    void SetGuid(
            const GUID_t& id);

};

class DelayedSnapshot
    : public LateJoinerData // Delayed Snapshot
{
    std::string description;
    bool if_someone;
    bool show_liveliness_;

public:

    DelayedSnapshot(
            const std::chrono::steady_clock::time_point tp,
            const std::string& desc,
            bool someone = true,
            bool show_liveliness = false)
        : LateJoinerData(tp)
        , description(desc)
        , if_someone(someone)
        , show_liveliness_(show_liveliness)
    {
    }

    ~DelayedSnapshot() override
    {
    }

    DelayedSnapshot() = delete;
    DelayedSnapshot(
            const DelayedSnapshot&) = default;
    DelayedSnapshot(
            DelayedSnapshot&&) = default;
    DelayedSnapshot& operator =(
            const DelayedSnapshot&) = default;
    DelayedSnapshot& operator =(
            DelayedSnapshot&&) = default;

    void operator ()(
            DiscoveryServerManager&) override;
};

// delayed construction of a new DataReader or DataWriter
template<class ReaderWriter>
void DelayedEndpointCreation<ReaderWriter>::operator ()(
        DiscoveryServerManager& manager)  /*override*/
{
    // Retrieve the corresponding participant
    DomainParticipant* part = manager.getParticipant(participant_guid);

    DomainEntity* pubsub = nullptr;

    manager.getPubSubEntityFromParticipantGuid<ReaderWriter>(participant_guid, pubsub);

    if (part == nullptr && owner_event != nullptr)
    {
        part = manager.getParticipant(owner_event->participant_guid);
    }

    if (!part)
    {
        // invalid participant
        LOG_ERROR(
            LateJoinerDataTraits<ReaderWriter>::endpoint_type <<
                " cannot be created because no participant is assigned.");
        return;
    }

    Topic* topic;

    // First we must register the type in the associated participant
    // Always register HelloWorld type
    TypeSupport hello_world_type_support(new HelloWorldPubSubType());
    hello_world_type_support.register_type(part);

    // If the topic is not defined, use builtin default topic (HelloWorld)
    if (type_name == "UNDEF")
    {
        topic = manager.getParticipantTopicByName(part,
                        DiscoveryServerManager::default_topic_description.name);
        if ( nullptr == topic)
        {
            topic = part->create_topic(DiscoveryServerManager::default_topic_description.name,
                            DiscoveryServerManager::default_topic_description.type_name,
                            part->get_default_topic_qos());
            manager.setParticipantTopic(part, topic);
        }
    }
    else
    {
        topic = manager.getParticipantTopicByName(part, topic_name);
        if ( nullptr == topic)
        {
            topic = part->create_topic_with_profile(topic_name,
                            type_name, topic_profile_name);
            manager.setParticipantTopic(part, topic);
        }

    }

    // Now we create the endpoint
    ReaderWriter* endpoint = LateJoinerDataTraits<ReaderWriter>::createEndpoint(pubsub, topic,
                    endpoint_profile_name, &manager);

    manager.setDomainEntityTopic(endpoint, topic);

    if (endpoint)
    {
        // update the associated DED if exists
        if (linked_destruction_event)
        {
            linked_destruction_event->SetGuid(endpoint->guid());
        }
        // and we update the state: DiscoveryServerManager::addDataWriter or DiscoveryServerManager::addDataReader
        (manager.*LateJoinerDataTraits<ReaderWriter>::add_endpoint_function)(endpoint);

        LOG_INFO(
            "New " << LateJoinerDataTraits<ReaderWriter>::endpoint_type << " created on participant " <<
                part->guid())
    }

}

template<class ReaderWriter>
void DelayedEndpointDestruction<ReaderWriter>::operator ()(
        DiscoveryServerManager& manager)  /*override*/
{
    // now we get the endpoint: DiscoveryServerManager::removeDataWriter or DiscoveryServerManager::removeDataReader
    ReaderWriter* endpoint =
            (manager.*LateJoinerDataTraits<ReaderWriter>::retrieve_endpoint_function)(endpoint_guid);

    if (endpoint)
    {
        GUID_t guid = endpoint->guid();
        (void)guid;

        ReturnCode_t ret;

        // and we removed the endpoint: Domain::removeDataWriter or Domain::removeDataReader
        ret = (manager.*LateJoinerDataTraits<ReaderWriter>::remove_endpoint_function)(endpoint);
        if (RETCODE_OK != ret)
        {
            LOG_ERROR("Error deleting Endpoint");
        }


        LOG_INFO(LateJoinerDataTraits<ReaderWriter>::endpoint_type << " called " << guid  << " destroyed ")
    }
}

// DelayedEndpointDestruction only knows its linked object guid after its creation
template<class ReaderWriter>
void DelayedEndpointDestruction<ReaderWriter>::SetGuid(
        const GUID_t& id)
{
    if (endpoint_guid == GUID_t::unknown())
    {
        endpoint_guid = id; // update
    }
}

template<class ReaderWriter>
DelayedEndpointCreation<ReaderWriter>& DelayedEndpointCreation<ReaderWriter>::operator =(
        DelayedEndpointCreation<ReaderWriter>&& d)
{
    LateJoinerData::operator =(d);
    participant_guid = std::move(d.participant_guid);
    linked_destruction_event = d.linked_destruction_event;
    owner_event = d.owner_event;
    topic_name = d.topic_name;
    type_name = d.type_name;
    topic_profile_name = d.topic_profile_name;
    endpoint_profile_name = d.endpoint_profile_name;

    return *this;
}

template<class ReaderWriter>
DelayedEndpointCreation<ReaderWriter>::DelayedEndpointCreation(
        DelayedEndpointCreation<ReaderWriter>&& d)
    : LateJoinerData(std::move(d))
{
    participant_guid = std::move(d.participant_guid);
    linked_destruction_event = d.linked_destruction_event;
    owner_event = d.owner_event;
    topic_name = d.topic_name;
    type_name = d.type_name;
    topic_profile_name = d.topic_profile_name;
    endpoint_profile_name = d.endpoint_profile_name;
}

template<class ReaderWriter>
DelayedEndpointCreation<ReaderWriter>::~DelayedEndpointCreation() /*override*/
{
}

class DelayedEnvironmentModification
    : public LateJoinerData // Delayed Environment Creation
{
    std::string new_value;
    std::pair<std::string, std::string> attributes;

public:

    GUID_t participant_guid; // participant GUID_t

    DelayedEnvironmentModification(
            const std::chrono::steady_clock::time_point tp,
            std::string key,
            std::string value)
        : LateJoinerData(tp)
        , attributes({key, value})
    {
    }

    ~DelayedEnvironmentModification() override
    {
    }

    DelayedEnvironmentModification() = delete;
    DelayedEnvironmentModification(
            const DelayedEnvironmentModification&) = default;
    DelayedEnvironmentModification(
            DelayedEnvironmentModification&&) = default;
    DelayedEnvironmentModification& operator =(
            const DelayedEnvironmentModification&) = default;
    DelayedEnvironmentModification& operator =(
            DelayedEnvironmentModification&&) = default;

    void operator ()(
            DiscoveryServerManager& ) override;
};

} // fastrtps
} // discovery_server

#endif // _LJ_H_
