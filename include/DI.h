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

#include <chrono>
#include <ctime>
#include <map>
#include <mutex>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include <fastrtps/rtps/common/Guid.h>

namespace tinyxml2 {
class XMLElement;
class XMLDocument;
} // namespace tinyxml2

namespace eprosima {
namespace discovery_server {

typedef fastrtps::rtps::GUID_t GUID_t;

//! common discovery info
struct DiscoveryItem
{
    typedef std::set<DiscoveryItem>::size_type size_type;

    //! enpoint identifier
    GUID_t endpoint_guid;

    DiscoveryItem(
            const GUID_t& id)
        : endpoint_guid(id)
    {
    }

    DiscoveryItem(
            GUID_t&& id)
        : endpoint_guid(std::move(id))
    {
    }

    DiscoveryItem(
            const DiscoveryItem&) = default;

    DiscoveryItem() = delete;
    DiscoveryItem(
            DiscoveryItem&&) = default;
    DiscoveryItem& operator =(
            const DiscoveryItem&) = default;
    DiscoveryItem& operator =(
            DiscoveryItem&&) = default;

    //! comparissons
    bool operator ==(
            const GUID_t&) const;
    bool operator !=(
            const GUID_t&) const;
    bool operator ==(
            const DiscoveryItem&) const;
    bool operator !=(
            const DiscoveryItem&) const;

    //! container ancillary
    bool operator <(
            const GUID_t&) const;
    bool operator <(
            const DiscoveryItem&) const;
};

//! publisher specific info
struct PublisherDiscoveryItem : public DiscoveryItem
{
    PublisherDiscoveryItem(
            const GUID_t& id,
            const std::string& type,
            const std::string& topic,
            const std::chrono::steady_clock::time_point& discovered_timestamp)
        : DiscoveryItem(id)
        , type_name(type)
        , topic_name(topic)
        , discovered_timestamp_(discovered_timestamp)
    {
    }

    PublisherDiscoveryItem(
            GUID_t&& id,
            std::string&& type,
            std::string&& topic,
            std::chrono::steady_clock::time_point&& discovered_timestamp)
        : DiscoveryItem(id)
        , type_name(std::move(type))
        , topic_name(std::move(topic))
        , discovered_timestamp_(std::move(discovered_timestamp))
    {
    }

    PublisherDiscoveryItem() = delete;
    PublisherDiscoveryItem(
            const PublisherDiscoveryItem&) = default;
    PublisherDiscoveryItem(
            PublisherDiscoveryItem&&) = default;
    PublisherDiscoveryItem& operator =(
            const PublisherDiscoveryItem&) = default;
    PublisherDiscoveryItem& operator =(
            PublisherDiscoveryItem&&) = default;

    //! comparissons
    bool operator ==(
            const PublisherDiscoveryItem&) const;

    //!Type name
    std::string type_name;

    //!Topic name
    std::string topic_name;

    //!Discovered timestamp
    std::chrono::steady_clock::time_point discovered_timestamp_;
};

std::ostream& operator <<(
        std::ostream&,
        const PublisherDiscoveryItem&);

//! subscriber specific info
struct SubscriberDiscoveryItem : public DiscoveryItem
{
    SubscriberDiscoveryItem(
            const GUID_t& id,
            const std::string& type,
            const std::string& topic,
            const std::chrono::steady_clock::time_point& discovered_timestamp)
        : DiscoveryItem(id)
        , type_name(type)
        , topic_name(topic)
        , discovered_timestamp_(discovered_timestamp)
    {
    }

    SubscriberDiscoveryItem(
            GUID_t&& id,
            std::string&& type,
            std::string&& topic,
            std::chrono::steady_clock::time_point&& discovered_timestamp)
        : DiscoveryItem(id)
        , type_name(std::move(type))
        , topic_name(std::move(topic))
        , discovered_timestamp_(std::move(discovered_timestamp))
    {
    }

    SubscriberDiscoveryItem() = delete;
    SubscriberDiscoveryItem(
            const SubscriberDiscoveryItem&) = default;
    SubscriberDiscoveryItem(
            SubscriberDiscoveryItem&&) = default;
    SubscriberDiscoveryItem& operator =(
            const SubscriberDiscoveryItem&) = default;
    SubscriberDiscoveryItem& operator =(
            SubscriberDiscoveryItem&&) = default;

    //!Type name
    std::string type_name;

    //!Topic name
    std::string topic_name;

    //!Discovered timestamp
    std::chrono::steady_clock::time_point discovered_timestamp_;

    //!Liveliness info
    int32_t alive_count{};
    int32_t not_alive_count{};

    //! comparissons
    bool operator ==(
            const SubscriberDiscoveryItem&) const;
};

std::ostream& operator <<(
        std::ostream&,
        const SubscriberDiscoveryItem&);

//! participant discovery info
struct ParticipantDiscoveryItem : public DiscoveryItem
{
    typedef std::set<PublisherDiscoveryItem> publisher_set;
    typedef std::set<SubscriberDiscoveryItem> subscriber_set;

    // identity
    bool is_server; // false -> client
    bool is_alive; // false if death already reported but owned endpoints yet to be
    std::string participant_name;
    std::chrono::steady_clock::time_point discovered_timestamp_;

    // local user entities
    publisher_set publishers;
    subscriber_set subscribers;

    ParticipantDiscoveryItem(
            const GUID_t& id,
            const std::string& name = std::string(),
            bool server = false,
            const std::chrono::steady_clock::time_point& discovered_timestamp = std::chrono::steady_clock::now())
        : DiscoveryItem(id)
        , is_server(server)
        , is_alive(true)
        , participant_name(name)
        , discovered_timestamp_(discovered_timestamp)
    {
    }

    ParticipantDiscoveryItem(
            GUID_t&& id,
            std::string&& name = std::string(),
            bool server = false,
            const std::chrono::steady_clock::time_point& discovered_timestamp = std::chrono::steady_clock::now())
        : DiscoveryItem(id)
        , is_server(server)
        , is_alive(true)
        , participant_name(name)
        , discovered_timestamp_(discovered_timestamp)
    {
    }

    ParticipantDiscoveryItem() = delete;
    ParticipantDiscoveryItem(
            const ParticipantDiscoveryItem&) = default;
    ParticipantDiscoveryItem(
            ParticipantDiscoveryItem&&) = default;
    ParticipantDiscoveryItem& operator =(
            const ParticipantDiscoveryItem&) = default;
    ParticipantDiscoveryItem& operator =(
            ParticipantDiscoveryItem&&) = default;

    // comparissons:

    using DiscoveryItem::operator ==;
    using DiscoveryItem::operator !=;

    /**
     * verifies if two PtDI keep the same info
     * despite been rooted on two different participants
     **/
    bool operator ==(
            const ParticipantDiscoveryItem&) const;
    bool operator !=(
            const ParticipantDiscoveryItem&) const;

    //! verifies if the given publisher was discovered by the participant
    bool operator [](
            const PublisherDiscoveryItem&) const;

    //! verifies if the given subscriber was discovered by the participant
    bool operator [](
            const SubscriberDiscoveryItem&) const;

    //! modify death acknowledge state
    void acknowledge(
            bool alive) const;

    // the get methods allows us to workaround STL constrain on sets
    // that makes all its iterators constant

    //! get publishers
    publisher_set& getPublishers() const
    {
        return const_cast<publisher_set&>(publishers);
    }

    //! get subscribers
    subscriber_set& getSubscribers() const
    {
        return const_cast<subscriber_set&>(subscribers);
    }

    void setName(
            const std::string& name) const
    {
        const_cast<std::string&>(participant_name) = name;
    }

    void setServer(
            bool& s) const
    {
        const_cast<bool&>(is_server) = s;
    }

    void setDiscoveredTimestamp(
            const std::chrono::steady_clock::time_point& discovered_timestamp) const
    {
        const_cast<std::chrono::steady_clock::time_point&>(discovered_timestamp_) = discovered_timestamp;
    }

    //! Returns the number of endpoints owned
    size_type CountEndpoints() const;
    size_type CountSubscribers() const;
    size_type CountPublishers() const;

};

std::ostream& operator <<(
        std::ostream&,
        const ParticipantDiscoveryItem&);

//! database, all discovery info associated with a participant
struct ParticipantDiscoveryDatabase : public DiscoveryItem, public std::set<ParticipantDiscoveryItem>
{
    typedef std::set<ParticipantDiscoveryItem>::size_type size_type;
    typedef ParticipantDiscoveryDatabase::iterator iterator;

    // we need a special iterator that ignores zombie members
    struct smart_iterator
        : std::iterator<
            std::forward_iterator_tag,
            iterator::value_type,
            iterator::difference_type,
            iterator::pointer,
            iterator::reference>
    {
        typedef ParticipantDiscoveryDatabase::iterator wrapped_iterator;

        smart_iterator(
                const ParticipantDiscoveryDatabase& cont);
        smart_iterator(
                const smart_iterator& it)
            : ref_cont_(it.ref_cont_)
            , wrap_it_(it.wrap_it_)
        {
        }

        smart_iterator operator ++();
        smart_iterator operator ++(
                int);

        reference operator *() const;

        pointer operator ->() const;
        bool operator ==(
                const smart_iterator& it) const;
        bool operator !=(
                const smart_iterator& it) const;

        smart_iterator end() const;

        const ParticipantDiscoveryDatabase& ref_cont_;
        wrapped_iterator wrap_it_;
    };

    ParticipantDiscoveryDatabase(
            const GUID_t& id )
        : DiscoveryItem(id)
    {
    }

    ParticipantDiscoveryDatabase(
            GUID_t&& id)
        : DiscoveryItem(id)
    {
    }

    ParticipantDiscoveryDatabase() = delete;
    ParticipantDiscoveryDatabase(
            const ParticipantDiscoveryDatabase&) = default;
    ParticipantDiscoveryDatabase(
            ParticipantDiscoveryDatabase&&) = default;
    ParticipantDiscoveryDatabase& operator =(
            const ParticipantDiscoveryDatabase&) = default;
    ParticipantDiscoveryDatabase& operator =(
            ParticipantDiscoveryDatabase&&) = default;

    smart_iterator sbegin() const;
    smart_iterator send() const;
    size_type real_size() const;

    size_type CountParticipants() const;
    size_type CountSubscribers() const;
    size_type CountPublishers() const;

};

bool operator ==(
        const ParticipantDiscoveryDatabase&,
        const ParticipantDiscoveryDatabase&);
std::ostream& operator <<(
        std::ostream&,
        const ParticipantDiscoveryDatabase&);

//! Snapshot, discovery info associated with all participants
struct Snapshot : public std::set<ParticipantDiscoveryDatabase>
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
    // show liveliness info
    bool show_liveliness_;

    // time conversions auxiliary
    static std::chrono::system_clock::time_point _sy_ck;
    static std::chrono::steady_clock::time_point _st_ck;
    static std::chrono::system_clock::time_point getSystemTime(
            std::chrono::steady_clock::time_point tp);
    static std::string getTimeStamp(
            std::chrono::steady_clock::time_point tp);

    // acceptable snapshot missalignment in ms
    static std::chrono::milliseconds aceptable_offset_;

    // description
    std::string _des;

    explicit Snapshot(
            std::chrono::steady_clock::time_point t = Snapshot::_st_ck,
            std::chrono::steady_clock::time_point pdp_cb = Snapshot::_st_ck,
            std::chrono::steady_clock::time_point edp_cb = Snapshot::_st_ck,
            bool someone = true,
            bool show_liveliness = false)
        : process_startup_(Snapshot::_st_ck)
        , _time(t)
        , last_PDP_callback_(pdp_cb)
        , last_EDP_callback_(edp_cb)
        , if_someone(someone)
        , show_liveliness_(show_liveliness)
    {
    }

    Snapshot(
            std::chrono::steady_clock::time_point t,
            std::chrono::steady_clock::time_point pdp_cb,
            std::chrono::steady_clock::time_point edp_cb,
            std::string& des,
            bool someone = true,
            bool show_liveliness = false)
        : process_startup_(Snapshot::_st_ck)
        , _time(t)
        , last_PDP_callback_(pdp_cb)
        , last_EDP_callback_(edp_cb)
        , if_someone(someone)
        , show_liveliness_(show_liveliness)
        , _des(des)
    {
    }

    Snapshot(
            const Snapshot&) = default;
    Snapshot(
            Snapshot&&) = default;
    Snapshot& operator =(
            const Snapshot&) = default;
    Snapshot& operator =(
            Snapshot&&) = default;
    Snapshot& operator +=(
            const Snapshot&);

    ParticipantDiscoveryDatabase& operator [](
            const GUID_t&);
    const ParticipantDiscoveryDatabase* operator [](
            const GUID_t&) const;

    void to_xml(
            tinyxml2::XMLElement* pRoot,
            tinyxml2::XMLDocument& xmlDoc) const;

    void from_xml(
            tinyxml2::XMLElement* pRoot);
};

std::ostream& operator <<(
        std::ostream&,
        const Snapshot&);

//! DiscoveryItemDatabase, auxiliary class to populate and manage Snapshots
class DiscoveryItemDatabase
{
    typedef ParticipantDiscoveryDatabase::size_type size_type;

    // reported discovery info
    Snapshot image; // each participant database info
    mutable std::mutex database_mutex; // atomic database operation

    // AddSubscriber and AddPublisher common implementation

    template<
        class T>
    bool AddEndPoint(T & (ParticipantDiscoveryItem::* m)() const,
            const GUID_t& spokesman,
            const GUID_t& ptid,
            const GUID_t& sid,
            const std::string& _typename,
            const std::string& topicname,
            const std::chrono::steady_clock::time_point& discovered_timestamp);

    template<
        class T>
    bool RemoveEndPoint(T & (ParticipantDiscoveryItem::* m)() const,
            const GUID_t& spokesman,
            const GUID_t& ptid,
            const GUID_t& sid);

public:

    DiscoveryItemDatabase()
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
    std::vector<const ParticipantDiscoveryItem*> FindParticipant(
            const GUID_t& ptid) const;

    //! Adds a new participant, returns false if allocation fails
    bool AddParticipant(
            const GUID_t& spokesman,
            const GUID_t& ptid,
            const std::string& name = std::string(),
            const std::chrono::steady_clock::time_point& discovered_timestamp = std::chrono::steady_clock::now(),
            bool server = false);

    //! Removes a participant, returns false if no there
    bool RemoveParticipant(
            const GUID_t& spokesman,
            const GUID_t& ptid);

    //! Removes a participant PtDB from the Snapshot
    bool RemoveParticipant(
            const GUID_t& deceased);

    //! Adds a new Subscriber, returns false if allocation fails
    bool AddSubscriber(
            const GUID_t& spokesman,
            const GUID_t& ptid,
            const GUID_t& sid,
            const std::string& _typename,
            const std::string& topicname,
            const std::chrono::steady_clock::time_point& discovered_timestamp);

    bool RemoveSubscriber(
            const GUID_t& spokesman,
            const GUID_t& ptid,
            const GUID_t& sid);

    //! Adds a new Publisher, returns false if allocation fails
    bool AddPublisher(
            const GUID_t& spokesman,
            const GUID_t& ptid,
            const GUID_t& pid,
            const std::string& _typename,
            const std::string& topicname,
            const std::chrono::steady_clock::time_point& discovered_timestamp);

    bool RemovePublisher(
            const GUID_t& spokesman,
            const GUID_t& ptid,
            const GUID_t& pid);

    void UpdateSubLiveliness(
            const GUID_t& subs,
            int32_t alive_count,
            int32_t not_alive_count);

    size_type CountParticipants(
            const GUID_t& spokesman ) const;
    size_type CountSubscribers(
            const GUID_t& spokesman ) const;
    size_type CountPublishers(
            const GUID_t& spokesman ) const;
    size_type CountSubscribers(
            const GUID_t& spokesman,
            const GUID_t& ptid) const;
    size_type CountPublishers(
            const GUID_t& spokesman,
            const GUID_t& ptid) const;

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
