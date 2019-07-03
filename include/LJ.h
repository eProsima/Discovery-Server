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
    std::chrono::steady_clock::time_point _time;

public:

    LJD(
        const std::chrono::steady_clock::time_point& tp)
        : _time(tp)
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
        { return _time < event._time; }

    void Wait() const
        { std::this_thread::sleep_until(_time); }
};

class DPC : public LJD // Delayed Participant Creation
{
    typedef void (DSManager::* addPart)(fastrtps::Participant *);

    ParticipantAttributes _atts;
    addPart _m;
    DPD * _pD;

public:

    GUID_t _guid; // participant GUID_t

    DPC(
            const std::chrono::steady_clock::time_point tp,
            fastrtps::ParticipantAttributes&& atts,
            addPart m,
            DPD* pD = nullptr)
        : LJD(tp)
        , _atts(std::move(atts))
        , _m(m)
        , _pD(pD) {}

    ~DPC() override {}

    DPC() = delete;
    DPC(const DPC&) = default;
    DPC(DPC&&) = default;
    DPC& operator=(const DPC&) = default;
    DPC& operator=(DPC&&) = default;

    void operator()(DSManager& ) override;
};

class DPD : public LJD // Delayed Participant Destruction
{
    GUID_t _id; // participant to remove

public:

    DPD(
        const std::chrono::steady_clock::time_point tp,
        GUID_t & id)
        : LJD(tp)
        , _id(id) {}

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
    typedef void (DSManager::* addEndpoint)(Publisher *);
    typedef Publisher * (DSManager::* getEndpoint)(GUID_t &);
    typedef Publisher * (*createEndpoint)(Participant* part, const PublisherAttributes&, PublisherListener*);
    typedef bool(*removeEndpoint)(Publisher*);

    static const std::string _endpoint_type;
    static const addEndpoint _ae;
    static const getEndpoint _ge;
    static const createEndpoint _ce;
    static const removeEndpoint _re;
};

template<> struct LJD_traits<Subscriber>
{
    typedef SubscriberAttributes Attributes;
    typedef void (DSManager::* addEndpoint)(Subscriber *);
    typedef Subscriber * (DSManager::* getEndpoint)(GUID_t &);
    typedef Subscriber * (*createEndpoint)(Participant* part, const SubscriberAttributes&, SubscriberListener*);
    typedef bool(*removeEndpoint)(Subscriber*);

    static const std::string _endpoint_type;
    static const addEndpoint _ae;
    static const getEndpoint _ge;
    static const createEndpoint _ce;
    static const removeEndpoint _re;
};

template<class PS> class DED;

template<class PS>
class DEC : public LJD // Delayed Enpoint Creation
{
    typedef typename LJD_traits<PS>::Attributes Attributes;
    Attributes * _atts;
    GUID_t _pid;
    DED<PS> * _pD; // destruction object
    DPC * _part; // associated participant event

public:
    DEC(
        const std::chrono::steady_clock::time_point tp,
        Attributes * atts,
        GUID_t & pid,
        DED<PS> * p = nullptr,
        DPC * part = nullptr)
        : LJD(tp)
        , _atts(atts)
        , _pid(pid)
        , _pD(p)
        , _part(part)
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
class DED : public LJD // Delayed Endpoint Destruction
{
    GUID_t _id; // endpoint to remove

public:

    DED(
            const std::chrono::steady_clock::time_point& tp,
            GUID_t id = GUID_t::unknown())
        : LJD(tp)
        , _id(id) 
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

class DS : public LJD // Delayed Snapshot
{
    std::string _desc;
    bool _someone;

public:
    DS(
        const std::chrono::steady_clock::time_point tp,
        const std::string& desc,
        bool someone = true)
        : LJD(tp)
        , _desc(desc)
        , _someone(true)
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
void DEC<PS>::operator()(DSManager & man) /*override*/
{
    // Retrieve the corresponding participant
    Participant* part = man.getParticipant(_pid);

    if (!part && _part)
    {
        part = man.getParticipant(_part->_guid);
    }

    if (!part)
    {   // invalid participant
        LOG_ERROR(LJD_traits<PS>::_endpoint_type << " cannot be created because no participant assign.");
        return;
    }

    // First we must register the type in the associated participant
    if (_atts->topic.getTopicName() == "UNDEF")
    {
        // fill in default topic
        _atts->topic = DSManager::_defaultTopic;

        // assure the participant has default type registered
        TopicDataType* pT = nullptr;
        if (!Domain::getRegisteredType(part, _atts->topic.topicDataType.c_str(), &pT))
        {
            Domain::registerType(part, &DSManager::_defaultType);
        }
    }
    else
    {
        // assure the participant has the type registered
        TopicDataType* pT = nullptr;
        std::string type_name = _atts->topic.topicDataType.to_string();
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
    PS * pEp = (*LJD_traits<PS>::_ce)(part, *_atts, nullptr);

    if (pEp)
    {
        // update the associated DED if exists
        if (_pD)
        {
            _pD->SetGuid(pEp->getGuid());
        }
        // and we update the state: DSManager::addPublisher or DSManager::addSubscriber
        (man.*LJD_traits<PS>::_ae)(pEp);

        LOG_INFO("New " << LJD_traits<PS>::_endpoint_type << " created on participant " << part->getGuid() )
    }

}

template<class PS>
void DED<PS>::operator()(DSManager & man) /*override*/
{
    // now we get the endpoint: DSManager::removePublisher or DSManager::removeSubscriber
    PS * p = (man.*LJD_traits<PS>::_ge)(_id);

    if (p)
    {
        GUID_t guid = p->getGuid();
        (void)guid;

        // and we removed the endpoint: Domain::removePublisher or Domain::removeSubscriber
        (*LJD_traits<PS>::_re)(p);

        LOG_INFO(LJD_traits<PS>::_endpoint_type << " called " << guid  << " destroyed ")
    }
}

// DED only knows its linked object guid after its creation
template<class PS>
void DED<PS>::SetGuid(const GUID_t& id)
{
    if (_id == GUID_t::unknown())
    {
        _id = id; // update
    }
}

template<class PS>
DEC<PS>& DEC<PS>::operator=(DEC<PS>&& d)
{
    LJD::operator=(d);
    _pid = std::move(d._pid);
    _pD = d._pD;
    _atts = d._atts;
    _part = d._part;
    d._atts = nullptr;

    return *this;
}

template<class PS>
DEC<PS>::DEC(DEC<PS>&& d)
    : LJD(std::move(d))
{
    _pid = std::move(d._pid);
    _pD = d._pD;
    _atts = d._atts;
    _part = d._part;
    d._atts = nullptr;
}

template<class PS>
DEC<PS>::~DEC() /*override*/
{
    if (_atts)
    {
        delete _atts;
    }
}

} // fastrtps
} // discovery_server

#endif // _LJ_H_
