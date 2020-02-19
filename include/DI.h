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


#ifndef _DI_H_
#define _DI_H_

#include <fastrtps/rtps/common/Guid.h>
#include <ctime>
#include <set>
#include <map>
#include <vector>
#include <mutex>
#include <chrono>
#include <string>
#include <ostream>

namespace tinyxml2 {
class XMLElement;
class XMLDocument;
}

namespace eprosima {
namespace discovery_server {

typedef fastrtps::rtps::GUID_t GUID_t;

//! common discovery info
struct DI
{
    typedef std::set<DI>::size_type size_type;

    //! enpoint identifier
    GUID_t endpoint_guid;

    DI(const GUID_t& id) : endpoint_guid(id) {}
    DI(GUID_t&& id) : endpoint_guid(std::move(id)) {}
    DI(const DI&) = default;

    DI() = delete;
    DI(DI&&) = default;
    DI& operator=(const DI&) = default;
    DI& operator=(DI&&) = default;

    //! comparissons
    bool operator==(const GUID_t&) const;
    bool operator!=(const GUID_t&) const;
    bool operator==(const DI&) const;
    bool operator!=(const DI&) const;

    //! container ancillary
    bool operator<(const GUID_t&) const;
    bool operator<(const DI&) const;
};

//! publisher specific info
struct PDI : public DI
{
    PDI(
        const GUID_t& id,
        const std::string& type,
        const std::string& topic)
        : DI(id)
        , type_name(type)
        , topic_name(topic)
    {
    }

    PDI(
        GUID_t&& id,
        std::string&& type,
        std::string&& topic)
        : DI(id)
        , type_name(std::move(type))
        , topic_name(std::move(topic))
    {
    }

    PDI() = delete;
    PDI(const PDI&) = default;
    PDI(PDI&&) = default;
    PDI& operator=(const PDI&) = default;
    PDI& operator=(PDI&&) = default;

    //! comparissons
    bool operator==(const PDI&) const;

    //!Type name
    std::string type_name;

    //!Topic name
    std::string topic_name;
};

std::ostream& operator<<(std::ostream&, const PDI&);

//! subscriber specific info
struct SDI : public DI
{
    SDI(
        const GUID_t& id,
        const std::string& type,
        const std::string& topic)
        : DI(id)
        , type_name(type)
        , topic_name(topic)
    {
    }

    SDI(
        GUID_t&& id,
        std::string&& type,
        std::string&& topic)
        : DI(id),
        type_name(std::move(type)),
        topic_name(std::move(topic))
    {
    }

    SDI() = delete;
    SDI(const SDI&) = default;
    SDI(SDI&&) = default;
    SDI& operator=(const SDI&) = default;
    SDI& operator=(SDI&&) = default;

    //!Type name
    std::string type_name;

    //!Topic name
    std::string topic_name;

    //!Liveliness info
    int32_t alive_count{};
    int32_t not_alive_count{};

    //! comparissons
    bool operator==(const SDI&) const;
};

std::ostream& operator<<(std::ostream&, const SDI&);

//! participant discovery info
struct PtDI : public DI
{
    typedef std::set<PDI> publisher_set;
    typedef std::set<SDI> subscriber_set;

    // identity
    bool is_server; // false -> client
    bool is_alive; // false if death already reported but owned endpoints yet to be
    std::string participant_name;

    // local user entities
    publisher_set publishers;
    subscriber_set subscribers;

    PtDI(
        const GUID_t& id,
        const std::string& name = std::string(),
        bool server = false)
        : DI(id),
        is_server(server),
        is_alive(true),
        participant_name(name)
    {
    }

    PtDI(GUID_t&& id,
        std::string&& name = std::string(),
        bool server = false)
        : DI(id),
        is_server(server),
        is_alive(true),
        participant_name(name)
    {
    }

    PtDI() = delete;
    PtDI(const PtDI&) = default;
    PtDI(PtDI&&) = default;
    PtDI& operator=(const PtDI&) = default;
    PtDI& operator=(PtDI&&) = default;

    // comparissons:

    using DI::operator==;
    using DI::operator!=;

    /**
    * verifies if two PtDI keep the same info
    * despite been rooted on two different participants
    **/
    bool operator==(const PtDI &) const;
    bool operator!=(const PtDI &) const;

    //! verifies if the given publisher was discovered by the participant
    bool operator[](const PDI &) const;

    //! verifies if the given subscriber was discovered by the participant
    bool operator[](const SDI &) const;

    //! modify death acknowledge state
    void acknowledge(bool alive) const;

    // the get methods allows us to workaround STL constrain on sets
    // that makes all its iterators constant

    //! get publishers
    publisher_set& getPublishers() const
    { 
        return const_cast<publisher_set &>(publishers);
    }

    //! get subscribers
    subscriber_set& getSubscribers() const
    {
        return const_cast<subscriber_set&>(subscribers);
    }

    void setName(const std::string & name) const 
    { 
        const_cast<std::string&>(participant_name) = name;
    }
    void setServer(bool & s) const 
    { 
        const_cast<bool &>(is_server) = s;
    }

    //! Returns the number of endpoints owned
    size_type CountEndpoints() const;
    size_type CountSubscribers() const;
    size_type CountPublishers() const;

};

std::ostream& operator<<(std::ostream&, const PtDI&);

//! database, all discovery info associated with a participant
struct PtDB : public DI, public std::set<PtDI>
{
    typedef std::set<PtDI>::size_type size_type;

    PtDB(
        const GUID_t& id )
        : DI(id)
    {
    }

    PtDB(
        GUID_t&& id)
        : DI(id)
    {
    }

    PtDB() = delete;
    PtDB(const PtDB&) = default;
    PtDB(PtDB&&) = default;
    PtDB& operator=(const PtDB&) = default;
    PtDB& operator=(PtDB&&) = default;

    size_type CountParticipants() const;
    size_type CountSubscribers() const;
    size_type CountPublishers() const;

};

bool operator==(const PtDB &, const  PtDB &);
std::ostream& operator<<(std::ostream&, const PtDB&);

//! Snapshot, discovery info associated with all participants
struct Snapshot : public std::set<PtDB>
{
    // process time
    std::chrono::steady_clock::time_point process_startup_;
    // snapshot time
    std::chrono::steady_clock::time_point _time;
    // last PDP callback time
    std::chrono::steady_clock::time_point last_PDP_callback_;
    // last EDP callback time
    std::chrono::steady_clock::time_point last_EDP_callback_;
    // report test framework that if nobody is discovered it should fail
    bool if_someone; 

    // time conversions auxiliary
    static std::chrono::system_clock::time_point _sy_ck;
    static std::chrono::steady_clock::time_point _st_ck;
    static std::chrono::system_clock::time_point getSystemTime(std::chrono::steady_clock::time_point tp);
    static std::string getTimeStamp(std::chrono::steady_clock::time_point tp);

    // acceptable snapshot missalignment in ms
    static std::chrono::milliseconds aceptable_offset_;

    // description
    std::string _des;

    explicit Snapshot(
        std::chrono::steady_clock::time_point t = Snapshot::_st_ck,
        std::chrono::steady_clock::time_point pdp_cb = Snapshot::_st_ck,
        std::chrono::steady_clock::time_point edp_cb = Snapshot::_st_ck,
        bool someone = true)
        : process_startup_(Snapshot::_st_ck)
        , _time(t)
        , last_PDP_callback_(pdp_cb)
        , last_EDP_callback_(edp_cb)
        , if_someone(someone)
    {
    }

    Snapshot(
        std::chrono::steady_clock::time_point t,
        std::chrono::steady_clock::time_point pdp_cb,
        std::chrono::steady_clock::time_point edp_cb,
        std::string & des,
        bool someone = true)
        : process_startup_(Snapshot::_st_ck)
        , _time(t)
        , last_PDP_callback_(pdp_cb)
        , last_EDP_callback_(edp_cb)
        , if_someone(someone)
        , _des(des)
    {
    }

    Snapshot(const Snapshot&) = default;
    Snapshot(Snapshot&&) = default;
    Snapshot& operator=(const Snapshot&) = default;
    Snapshot& operator=(Snapshot&&) = default;
    Snapshot& operator+=(const Snapshot&);

    PtDB & operator[](const GUID_t &);
    const PtDB * operator[](const GUID_t &) const;

    void to_xml(
        tinyxml2::XMLElement* pRoot,
        tinyxml2::XMLDocument& xmlDoc) const;

    void from_xml(tinyxml2::XMLElement* pRoot);
};

std::ostream& operator<<(std::ostream&, const Snapshot&);

//! DI_database, auxiliary class to populate and manage Snapshots
class DI_database
{
    typedef PtDB::size_type size_type;

    // reported discovery info
    Snapshot image; // each participant database info
    mutable std::mutex database_mutex; // atomic database operation

    // AddSubscriber and AddPublisher common implementation

    template<
        class T>
        bool AddEndPoint(T&(PtDI::* m)() const,
            const GUID_t& spokesman,
            const GUID_t & ptid,
            const GUID_t & sid,
            const std::string & _typename,
            const std::string & topicname);

    template<
        class T>
        bool RemoveEndPoint(T&(PtDI::* m)() const,
            const GUID_t& spokesman,
            const GUID_t & ptid,
            const GUID_t & sid);

public:
    DI_database()
        : image(std::chrono::steady_clock::now()
        , std::chrono::steady_clock::now())
    {
    }

    //! Get Snapshot time
    std::chrono::steady_clock::time_point getTime() const
    { 
        return image._time;
    }

    //! Returns a pointer to the PtDI or null if not found
    std::vector<const PtDI*> FindParticipant(const GUID_t & ptid) const;

    //! Adds a new participant, returns false if allocation fails
    bool AddParticipant(const GUID_t& spokesman,
        const GUID_t & ptid,
        const std::string& name = std::string(),
        bool server = false);

    //! Removes a participant, returns false if no there
    bool RemoveParticipant(const GUID_t& spokesman,
        const GUID_t & ptid);

    //! Removes a participant PtDB from the Snapshot
    bool RemoveParticipant(const GUID_t& deceased);

    //! Adds a new Subscriber, returns false if allocation fails
    bool AddSubscriber(const GUID_t& spokesman,
        const GUID_t & ptid,
        const GUID_t & sid,
        const std::string& _typename,
        const std::string & topicname);

    bool RemoveSubscriber(const GUID_t& spokesman,
        const GUID_t & ptid,
        const GUID_t & sid);

    //! Adds a new Publisher, returns false if allocation fails
    bool AddPublisher(const GUID_t& spokesman,
        const GUID_t & ptid,
        const GUID_t & pid,
        const std::string & _typename,
        const std::string & topicname);

    bool RemovePublisher(const GUID_t& spokesman,
        const GUID_t & ptid,
        const GUID_t & pid);

    void UpdateSubLiveliness(const GUID_t & subs,
        int32_t alive_count,
        int32_t not_alive_count);

    size_type CountParticipants(const GUID_t& spokesman ) const;
    size_type CountSubscribers(const GUID_t& spokesman ) const;
    size_type CountPublishers(const GUID_t& spokesman ) const;
    size_type CountSubscribers(const GUID_t& spokesman, const GUID_t & ptid) const;
    size_type CountPublishers(const GUID_t& spokesman, const GUID_t & ptid) const;

    // Get a copy the current SnapShot
    Snapshot GetState() const
    {
        std::lock_guard<std::mutex> lock(database_mutex);
        return image;
    }

};

} // fastrtps
} // discovery_server

#endif // _DI_H_
