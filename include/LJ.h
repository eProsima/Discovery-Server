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
#include <fastrtps/rtps/common/Guid.h>
#include <fastrtps/types/DynamicPubSubType.h>
#include <fastrtps/Domain.h>
#include "DSManager.h"

#include <thread>

namespace eprosima {
namespace discovery_server {

using eprosima::fastrtps::rtps::GUID_t;
using eprosima::fastrtps::Participant;
using eprosima::fastrtps::Publisher;
using eprosima::fastrtps::Subscriber;
using eprosima::fastrtps::SubscriberListener;
using eprosima::fastrtps::PublisherListener;
using eprosima::fastrtps::PublisherAttributes;
using eprosima::fastrtps::SubscriberAttributes;
using eprosima::fastrtps::ParticipantAttributes;

class DPD;

class LJD // Late Joiner Data basic class
{
    // When the late joiner should be added
    std::chrono::steady_clock::time_point time;

public:

    LJD(
        const std::chrono::steady_clock::time_point& tp)
        : time(tp)
    {
    }

    virtual ~LJD() {};

    LJD() = delete;
    LJD(const LJD&) = default;
    LJD(LJD&&) = default;
    LJD& operator=(const LJD&) = default;
    LJD& operator=(LJD&&) = default;

    // Which operation should I do
    virtual void operator()(DSManager& ) = 0;

    bool operator<(const LJD & event) const
    {
        return time < event.time;
    }

    void Wait() const
     {
        std::this_thread::sleep_until(time);
    }
};

class DPC
    : public LJD // Delayed Participant Creation
{
    typedef void (DSManager::* AddParticipant)(fastrtps::Participant *);

    ParticipantAttributes attributes;
    AddParticipant participant_creation_function;
    DPD * removal_event;

public:

    GUID_t participant_guid; // participant GUID_t

    DPC(
            const std::chrono::steady_clock::time_point tp,
            fastrtps::ParticipantAttributes&& atts,
            AddParticipant m,
            DPD* pD = nullptr)
        : LJD(tp)
        , attributes(std::move(atts))
        , participant_creation_function(m)
        , removal_event(pD) {}

    ~DPC() override {}

    DPC() = delete;
    DPC(const DPC&) = default;
    DPC(DPC&&) = default;
    DPC& operator=(const DPC&) = default;
    DPC& operator=(DPC&&) = default;

    void operator()(DSManager& ) override;
};

class DPD
    : public LJD // Delayed Participant Destruction
{
    GUID_t participant_id; // participant to remove

public:

    DPD(
        const std::chrono::steady_clock::time_point tp,
        GUID_t & id)
        : LJD(tp)
        , participant_id(id) {}

    ~DPD() override {}

    DPD() = delete;
    DPD(const DPD&) = default;
    DPD(DPD&&) = default;
    DPD& operator=(const DPD&) = default;
    DPD& operator=(DPD&&) = default;

    void operator()(DSManager &) override;
    void SetGuid(const GUID_t &);

};

template<class PS> struct LJD_traits
{
};

template<> struct LJD_traits<Publisher>
{
    typedef PublisherAttributes Attributes;
    typedef void (DSManager::* AddEndpoint)(Publisher *);
    typedef Publisher * (DSManager::* GetEndpoint)(GUID_t &);
    typedef Publisher * (*CreateEndpoint)(Participant* part, const PublisherAttributes&, PublisherListener*);
    typedef bool(*removeEndpoint)(Publisher*);

    static const std::string endpoint_type;
    static const AddEndpoint add_endpoint_function;
    static const GetEndpoint retrieve_endpoint_function;
    static const CreateEndpoint create_endpoint_function;
    static const removeEndpoint remove_endpoint_function;
};

template<> struct LJD_traits<Subscriber>
{
    typedef SubscriberAttributes Attributes;
    typedef void (DSManager::* AddEndpoint)(Subscriber *);
    typedef Subscriber * (DSManager::* GetEndpoint)(GUID_t &);
    typedef Subscriber * (*CreateEndpoint)(Participant* part, const SubscriberAttributes&, SubscriberListener*);
    typedef bool(*removeEndpoint)(Subscriber*);

    static const std::string endpoint_type;
    static const AddEndpoint add_endpoint_function;
    static const GetEndpoint retrieve_endpoint_function;
    static const CreateEndpoint create_endpoint_function;
    static const removeEndpoint remove_endpoint_function;
};

template<class PS> class DED;

template<class PS>
class DEC
    : public LJD // Delayed Enpoint Creation
{
    typedef typename LJD_traits<PS>::Attributes Attributes;
    Attributes * participant_attributes;
    GUID_t participant_guid;
    DED<PS> * linked_destruction_event; 
    DPC * owner_event; // associated participant event

public:
    DEC(
        const std::chrono::steady_clock::time_point tp,
        Attributes * atts,
        GUID_t & pid,
        DED<PS> * p = nullptr,
        DPC * part = nullptr)
        : LJD(tp)
        , participant_attributes(atts)
        , participant_guid(pid)
        , linked_destruction_event(p)
        , owner_event(part)
    {
    }

    DEC(DEC&&);
    ~DEC() override;
    DEC& operator=(DEC&& d);

    DEC() = delete;
    DEC(const DEC&) = default;
    DEC& operator=(const DEC&) = default;

    void operator()(DSManager &) override;

};

template<class PS>
class DED
    : public LJD // Delayed Endpoint Destruction
{
    GUID_t endpoint_guid; // endpoint to remove

public:

    DED(
            const std::chrono::steady_clock::time_point& tp,
            GUID_t id = GUID_t::unknown())
        : LJD(tp)
        , endpoint_guid(id) 
    {}

    ~DED() override {}

    DED() = delete;
    DED(const DED&) = default;
    DED(DED&&) = default;
    DED& operator=(const DED&) = default;
    DED& operator=(DED&&) = default;

    void operator()(DSManager &) override;
    void SetGuid(const GUID_t& id);

};

class DS
    : public LJD // Delayed Snapshot
{
    std::string description;
    bool if_someone;

public:
    DS(
        const std::chrono::steady_clock::time_point tp,
        const std::string& desc,
        bool someone = true)
        : LJD(tp)
        , description(desc)
        , if_someone(someone)
    {
    }

    ~DS() override {}

    DS() = delete;
    DS(const DS&) = default;
    DS(DS&&) = default;
    DS& operator=(const DS&) = default;
    DS& operator=(DS&&) = default;

    void operator()(DSManager &) override;
};

// delayed construction of a new subscriber or publisher
template<class PS>
void DEC<PS>::operator()(
        DSManager & man) /*override*/
{
    // Retrieve the corresponding participant
    Participant* part = man.getParticipant(participant_guid);

    if (part == nullptr && owner_event != nullptr)
    {
        part = man.getParticipant(owner_event->participant_guid);
    }

    if (!part)
    {   // invalid participant
        LOG_ERROR(LJD_traits<PS>::endpoint_type << " cannot be created because no participant assign.");
        return;
    }

    // First we must register the type in the associated participant
    if (participant_attributes->topic.getTopicName() == "UNDEF")
    {
        // fill in default topic
        participant_attributes->topic = DSManager::builtin_defaultTopic;

        // assure the participant has default type registered
        TopicDataType* pT = nullptr;
        if (!Domain::getRegisteredType(part, participant_attributes->topic.topicDataType.c_str(), &pT))
        {
            Domain::registerType(part, &DSManager::builtin_defaultType);
        }
    }
    else
    {
        // assure the participant has the type registered
        TopicDataType* pT = nullptr;
        std::string type_name = participant_attributes->topic.topicDataType.to_string();
        if (!Domain::getRegisteredType(part, type_name.c_str(), &pT))
        {
            eprosima::fastrtps::types::DynamicPubSubType* pDt = man.setType(type_name);

            if (pDt)
            {
                // register it
                // Domain::registerDynamicType(part, pDt);
                Domain::registerType(part, static_cast<TopicDataType*>(pDt));
            }
        }
    }

    // Now we create the endpoint: Domain::createSubscriber or createPublisher
    PS * pEp = (*LJD_traits<PS>::create_endpoint_function)(part, *participant_attributes, nullptr);

    if (pEp)
    {
        // update the associated DED if exists
        if (linked_destruction_event)
        {
            linked_destruction_event->SetGuid(pEp->getGuid());
        }
        // and we update the state: DSManager::addPublisher or DSManager::addSubscriber
        (man.*LJD_traits<PS>::add_endpoint_function)(pEp);

        LOG_INFO("New " << LJD_traits<PS>::endpoint_type << " created on participant " << part->getGuid() )
    }

}

template<class PS>
void DED<PS>::operator()(
        DSManager & man) /*override*/
{
    // now we get the endpoint: DSManager::removePublisher or DSManager::removeSubscriber
    PS * p = (man.*LJD_traits<PS>::retrieve_endpoint_function)(endpoint_guid);

    if (p)
    {
        GUID_t guid = p->getGuid();
        (void)guid;

        // and we removed the endpoint: Domain::removePublisher or Domain::removeSubscriber
        (*LJD_traits<PS>::remove_endpoint_function)(p);

        LOG_INFO(LJD_traits<PS>::endpoint_type << " called " << guid  << " destroyed ")
    }
}

// DED only knows its linked object guid after its creation
template<class PS>
void DED<PS>::SetGuid(
        const GUID_t& id)
{
    if (endpoint_guid == GUID_t::unknown())
    {
        endpoint_guid = id; // update
    }
}

template<class PS>
DEC<PS>& DEC<PS>::operator=(
        DEC<PS>&& d)
{
    LJD::operator=(d);
    participant_guid = std::move(d.participant_guid);
    linked_destruction_event = d.linked_destruction_event;
    participant_attributes = d.participant_attributes;
    owner_event = d.owner_event;
    d.participant_attributes = nullptr;

    return *this;
}

template<class PS>
DEC<PS>::DEC(
        DEC<PS>&& d)
    : LJD(std::move(d))
{
    participant_guid = std::move(d.participant_guid);
    linked_destruction_event = d.linked_destruction_event;
    participant_attributes = d.participant_attributes;
    owner_event = d.owner_event;
    d.participant_attributes = nullptr;
}

template<class PS>
DEC<PS>::~DEC() /*override*/
{
    if (participant_attributes)
    {
        delete participant_attributes;
    }
}

} // fastrtps
} // discovery_server

#endif // _LJ_H_
