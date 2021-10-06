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

#include <fastrtps/rtps/common/Guid.h>
#include <fastrtps/types/DynamicPubSubType.h>
#include <fastrtps/xmlparser/XMLProfileManager.h>

#include "DSManager.h"

namespace eprosima {
namespace discovery_server {

using eprosima::fastrtps::rtps::GUID_t;
using eprosima::fastrtps::PublisherAttributes;
using eprosima::fastrtps::SubscriberAttributes;
using eprosima::fastrtps::ParticipantAttributes;

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
            DSManager& ) = 0;

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
    typedef void (DSManager::* AddParticipant)(
            fastdds::dds::DomainParticipant*);

    ParticipantAttributes attributes;
    AddParticipant participant_creation_function;
    DelayedParticipantDestruction* removal_event;

public:

    GUID_t participant_guid; // participant GUID_t

    DelayedParticipantCreation(
            const std::chrono::steady_clock::time_point tp,
            fastrtps::ParticipantAttributes&& atts,
            AddParticipant m,
            DelayedParticipantDestruction* pD = nullptr)
        : LateJoinerData(tp)
        , attributes(std::move(atts))
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
            DSManager& ) override;
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
            DSManager&) override;
    void SetGuid(
            const GUID_t&);

};

template<class PublisherSubscriber> struct LateJoinerDataTraits
{
};

template<> struct LateJoinerDataTraits<DataWriter>
{
    typedef PublisherAttributes Attributes;
    typedef void (DSManager::* AddEndpoint)(
            DataWriter*);
    typedef DataWriter* (DSManager::* GetEndpoint)(
            GUID_t&);
    typedef ReturnCode_t (DSManager::* removeEndpoint)(
            DataWriter*);

    static const std::string endpoint_type;
    static const AddEndpoint add_endpoint_function;
    static const GetEndpoint retrieve_endpoint_function;
    static const removeEndpoint remove_endpoint_function;

    // we don't want yet to listen on publisher callbacks
    static DataWriter* createEndpoint(
            fastdds::dds::DomainEntity* part,
            fastdds::dds::Topic* topic,
            const PublisherAttributes&,
            void* = nullptr);
};

template<> struct LateJoinerDataTraits<DataReader>
{
    typedef SubscriberAttributes Attributes;
    typedef void (DSManager::* AddEndpoint)(
            DataReader*);
    typedef DataReader* (DSManager::* GetEndpoint)(
            GUID_t&);
    typedef ReturnCode_t (DSManager::* removeEndpoint)(
            DataReader*);

    static const std::string endpoint_type;
    static const AddEndpoint add_endpoint_function;
    static const GetEndpoint retrieve_endpoint_function;
    static const removeEndpoint remove_endpoint_function;

    static DataReader* createEndpoint(
            fastdds::dds::DomainEntity* part,
            fastdds::dds::Topic* topic,
            const SubscriberAttributes&,
            fastdds::dds::SubscriberListener* = nullptr);
};

template<class PublisherSubscriber> class DelayedEndpointDestruction;

template<class PublisherSubscriber>
class DelayedEndpointCreation
    : public LateJoinerData // Delayed Enpoint Creation
{
    typedef typename LateJoinerDataTraits<PublisherSubscriber>::Attributes Attributes;
    Attributes* participant_attributes;
    GUID_t participant_guid;
    DelayedEndpointDestruction<PublisherSubscriber>* linked_destruction_event;
    DelayedParticipantCreation* owner_event;  // associated participant event

public:

    DelayedEndpointCreation(
            const std::chrono::steady_clock::time_point tp,
            Attributes* atts,
            GUID_t& pid,
            DelayedEndpointDestruction<PublisherSubscriber>* p = nullptr,
            DelayedParticipantCreation* part = nullptr)
        : LateJoinerData(tp)
        , participant_attributes(atts)
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
            DSManager&) override;

};

template<class PublisherSubscriber>
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
            DSManager&) override;
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
            DSManager&) override;
};

// delayed construction of a new subscriber or publisher
template<class PublisherSubscriber>
void DelayedEndpointCreation<PublisherSubscriber>::operator ()(
        DSManager& man)  /*override*/
{
    // Retrieve the corresponding participant
    DomainParticipant* part = man.getParticipant(participant_guid);

    DomainEntity* pubsub = nullptr;

    man.getPubSubEntityFromParticipantGuid<PublisherSubscriber>(participant_guid, pubsub);

    if (part == nullptr && owner_event != nullptr)
    {
        part = man.getParticipant(owner_event->participant_guid);
    }

    if (!part)
    {
        // invalid participant
        LOG_ERROR(
            LateJoinerDataTraits<PublisherSubscriber>::endpoint_type <<
                " cannot be created because no participant assign.");
        return;
    }

    Topic* topic = nullptr;

    // First we must register the type in the associated participant
    if (participant_attributes->topic.getTopicName() == "UNDEF")
    {

        // fill in default topic
        participant_attributes->topic = DSManager::builtin_defaultTopic;

        // assure the participant has default type registered
        if (part->find_type(participant_attributes->topic.topicDataType.c_str()).empty())
        {

            TypeSupport ts(new HelloWorldPubSubType());
            ts.register_type(part);
        }

        topic = man.getParticipantTopicByName(part, DSManager::builtin_defaultTopic.getTopicName().to_string());
        if ( nullptr == topic)
        {
            topic = part->create_topic(DSManager::builtin_defaultTopic.getTopicName().to_string(),
                            DSManager::builtin_defaultTopic.topicDataType.to_string(), part->get_default_topic_qos());
            man.setParticipantTopic(part, topic);
        }
    }
    else
    {

        std::string type_name = participant_attributes->topic.topicDataType.to_string();
        if (part->find_type(participant_attributes->topic.topicDataType.c_str()).empty())
        {
            eprosima::fastrtps::types::DynamicPubSubType* pDt = man.setType(type_name);

            if (pDt)
            {

                eprosima::fastrtps::types::DynamicType_ptr dyn_type =
                        eprosima::fastrtps::xmlparser::XMLProfileManager::getDynamicTypeByName(
                    participant_attributes->topic.topicDataType.to_string())->build();

                TypeSupport ts(dyn_type);
                ts.register_type(part);
            }
        }

        topic = man.getParticipantTopicByName(part, participant_attributes->topic.getTopicName().to_string());
        if ( nullptr == topic)
        {
            topic = part->create_topic(participant_attributes->topic.getTopicName().to_string(),
                            participant_attributes->topic.topicDataType.to_string(), part->get_default_topic_qos());
            man.setParticipantTopic(part, topic);
        }

    }

    // Now we create the endpoint: Domain::createSubscriber or createPublisher
    PublisherSubscriber* pEp = LateJoinerDataTraits<PublisherSubscriber>::createEndpoint(pubsub, topic,
                    *participant_attributes, &man);

    man.setDomainEntityTopic(pEp, topic);



    if (pEp)
    {
        // update the associated DED if exists
        if (linked_destruction_event)
        {
            linked_destruction_event->SetGuid(pEp->guid());
        }
        // and we update the state: DSManager::addPublisher or DSManager::addSubscriber
        (man.*LateJoinerDataTraits<PublisherSubscriber>::add_endpoint_function)(pEp);

        LOG_INFO(
            "New " << LateJoinerDataTraits<PublisherSubscriber>::endpoint_type << " created on participant " <<
                part->guid())
    }

}

template<class PublisherSubscriber>
void DelayedEndpointDestruction<PublisherSubscriber>::operator ()(
        DSManager& man)  /*override*/
{
    // now we get the endpoint: DSManager::removePublisher or DSManager::removeSubscriber
    PublisherSubscriber* p =
            (man.*LateJoinerDataTraits<PublisherSubscriber>::retrieve_endpoint_function)(endpoint_guid);

    if (p)
    {
        GUID_t guid = p->guid();
        (void)guid;

        ReturnCode_t ret;

        // and we removed the endpoint: Domain::removePublisher or Domain::removeSubscriber
        ret = (man.*LateJoinerDataTraits<PublisherSubscriber>::remove_endpoint_function)(p);
        if (ReturnCode_t::RETCODE_OK != ret)
        {
            LOG_ERROR("Error deleting Endpoint");
        }


        LOG_INFO(LateJoinerDataTraits<PublisherSubscriber>::endpoint_type << " called " << guid  << " destroyed ")
    }
}

// DED only knows its linked object guid after its creation
template<class PublisherSubscriber>
void DelayedEndpointDestruction<PublisherSubscriber>::SetGuid(
        const GUID_t& id)
{
    if (endpoint_guid == GUID_t::unknown())
    {
        endpoint_guid = id; // update
    }
}

template<class PublisherSubscriber>
DelayedEndpointCreation<PublisherSubscriber>& DelayedEndpointCreation<PublisherSubscriber>::operator =(
        DelayedEndpointCreation<PublisherSubscriber>&& d)
{
    LateJoinerData::operator =(d);
    participant_guid = std::move(d.participant_guid);
    linked_destruction_event = d.linked_destruction_event;
    participant_attributes = d.participant_attributes;
    owner_event = d.owner_event;
    d.participant_attributes = nullptr;

    return *this;
}

template<class PublisherSubscriber>
DelayedEndpointCreation<PublisherSubscriber>::DelayedEndpointCreation(
        DelayedEndpointCreation<PublisherSubscriber>&& d)
    : LateJoinerData(std::move(d))
{
    participant_guid = std::move(d.participant_guid);
    linked_destruction_event = d.linked_destruction_event;
    participant_attributes = d.participant_attributes;
    owner_event = d.owner_event;
    d.participant_attributes = nullptr;
}

template<class PublisherSubscriber>
DelayedEndpointCreation<PublisherSubscriber>::~DelayedEndpointCreation() /*override*/
{
    if (participant_attributes)
    {
        delete participant_attributes;
    }
}

} // fastrtps
} // discovery_server

#endif // _LJ_H_
