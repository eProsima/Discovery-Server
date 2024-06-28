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

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
#include <sstream>

#include <tinyxml2.h>

#include "DiscoveryItem.h"
#include "IDs.h"
#include "log/DSLog.h"

#ifndef XMLCheckResult
#define XMLCheckResult(a_eResult) if (a_eResult != XML_SUCCESS){ printf("Error: %i\n", a_eResult); \
                                                                 return a_eResult; }
#endif // ifndef XMLCheckResult

using namespace eprosima::fastdds;
using namespace eprosima::discovery_server;

// basic discovery items operations

bool DiscoveryItem::operator ==(
        const GUID_t& guid) const
{
    return endpoint_guid == guid;
}

bool DiscoveryItem::operator !=(
        const GUID_t& guid) const
{
    return endpoint_guid != guid;
}

bool DiscoveryItem::operator ==(
        const DiscoveryItem& d) const
{
    return endpoint_guid == d.endpoint_guid;
}

bool DiscoveryItem::operator !=(
        const DiscoveryItem& d) const
{
    return endpoint_guid != d.endpoint_guid;
}

bool DiscoveryItem::operator <(
        const GUID_t& guid) const
{
    return endpoint_guid < guid;
}

bool DiscoveryItem::operator <(
        const DiscoveryItem& d) const
{
    return endpoint_guid < d.endpoint_guid;
}

// DataWriter discovery item operations
bool DataWriterDiscoveryItem::operator ==(
        const DataWriterDiscoveryItem& p) const
{
    return DiscoveryItem::operator ==(p)
           && type_name == p.type_name
           && topic_name == p.topic_name;
}

std::ostream& eprosima::discovery_server::operator <<(
        std::ostream& os,
        const DataWriterDiscoveryItem& di)
{
    return os << "DataWriter " << di.endpoint_guid << " TypeName: " << di.type_name
              << " TopicName: " << di.topic_name;
}

// DataReader discovery item operations
bool DataReaderDiscoveryItem::operator ==(
        const DataReaderDiscoveryItem& p) const
{
    return DiscoveryItem::operator ==(p)
           && type_name == p.type_name
           && topic_name == p.topic_name;
}

std::ostream& eprosima::discovery_server::operator <<(
        std::ostream& os,
        const DataReaderDiscoveryItem& di)
{
    return os << "DataReader " << di.endpoint_guid << " TypeName: " << di.type_name
              << " TopicName: " << di.topic_name << " liveliness, alive_count: " << di.alive_count
              << " not_alive_count: " << di.not_alive_count;
}

// participant discovery item operations

bool ParticipantDiscoveryItem::operator ==(
        const ParticipantDiscoveryItem& p) const
{
    return DiscoveryItem::operator ==(p)
           && this->datawriters == p.datawriters
           && this->datareaders == p.datareaders;
}

bool ParticipantDiscoveryItem::operator !=(
        const ParticipantDiscoveryItem& p) const
{
    return DiscoveryItem::operator !=(p)
           || this->datawriters != p.datawriters
           || this->datareaders != p.datareaders;
}

std::ostream& eprosima::discovery_server::operator <<(
        std::ostream& os,
        const ParticipantDiscoveryItem& di)
{
    os << (di.is_alive ? "\t" : "\t Zombie" ) << " Participant ";

    if (!di.participant_name.empty())
    {
        os << di.participant_name << ' ';
    }

    os << di.endpoint_guid;

    if ( di.CountEndpoints() > 0 )
    {
        os << " has:" << std::endl;
    }

    if (di.datawriters.size())
    {
        os << "\t\t" << di.datawriters.size() << " datawriters:" << std::endl;

        for ( const DataWriterDiscoveryItem& pdi : di.datawriters )
        {
            os << "\t\t\t" << pdi << std::endl;
        }
    }

    if (di.datareaders.size())
    {
        os << "\t\t" << di.datareaders.size() << " datareaders:" << std::endl;

        for (const DataReaderDiscoveryItem& sdi : di.datareaders)
        {
            os << "\t\t\t" << sdi << std::endl;
        }
    }

    return os;
}

bool ParticipantDiscoveryItem::operator [](
        const DataWriterDiscoveryItem& p) const
{
    // search the list
    return datawriters.end() != datawriters.find(p);
}

bool ParticipantDiscoveryItem::operator [](
        const DataReaderDiscoveryItem& p) const
{
    // search the list
    return datareaders.end() != datareaders.find(p);
}

void ParticipantDiscoveryItem::acknowledge(
        bool alive) const
{
    // STL makes iterator const to prevent that any key changing unsorts the container
    // so we introduce this method to avoid constant ugly const_cast use
    ParticipantDiscoveryItem& part = const_cast<ParticipantDiscoveryItem&>(*this);
    part.is_alive = alive;
}

ParticipantDiscoveryItem::size_type ParticipantDiscoveryItem::CountDataReaders() const
{
    return datareaders.size();
}

ParticipantDiscoveryItem::size_type ParticipantDiscoveryItem::CountDataWriters() const
{
    return datawriters.size();
}

ParticipantDiscoveryItem::size_type ParticipantDiscoveryItem::CountEndpoints() const
{
    return datawriters.size() + datareaders.size();
}

ParticipantDiscoveryDatabase::size_type ParticipantDiscoveryDatabase::CountParticipants() const
{
    return size();
}

ParticipantDiscoveryDatabase::size_type ParticipantDiscoveryDatabase::CountDataReaders() const
{
    return std::accumulate(begin(), end(), size_type(0),
                   [](size_type subs, const ParticipantDiscoveryItem& part)
                   {
                       return subs + part.CountDataReaders();
                   }
                   );
}

ParticipantDiscoveryDatabase::size_type ParticipantDiscoveryDatabase::CountDataWriters() const
{
    return std::accumulate(begin(), end(), size_type(0),
                   [](size_type pubs, const ParticipantDiscoveryItem& part)
                   {
                       return pubs + part.CountDataWriters();
                   }
                   );
}

std::ostream& eprosima::discovery_server::operator <<(
        std::ostream& os,
        const ParticipantDiscoveryDatabase& db)
{
    os << "Participant " << db.endpoint_guid << " discovered " << db.CountParticipants() << " participants, ";
    os << db.CountDataWriters() << " datawriters and " << db.CountDataReaders() << " datareaders:" << std::endl;

    for (const ParticipantDiscoveryItem& pt : db)
    {
        os << pt << std::endl;
    }

    return os;
}

// wrap_iterator implementation

ParticipantDiscoveryDatabase::smart_iterator ParticipantDiscoveryDatabase::sbegin() const
{
    return smart_iterator(*this);
}

ParticipantDiscoveryDatabase::smart_iterator ParticipantDiscoveryDatabase::send() const
{
    return smart_iterator(*this).end();
}

ParticipantDiscoveryDatabase::size_type ParticipantDiscoveryDatabase::real_size() const
{
    return std::distance(sbegin(), send());
}

ParticipantDiscoveryDatabase::smart_iterator::smart_iterator(
        const ParticipantDiscoveryDatabase& cont)
    : ref_cont_(cont)
    , wrap_it_(cont.begin())
{
    // ignore zombies
    while ( wrap_it_ != cont.end() && !wrap_it_->is_alive )
    {
        ++wrap_it_;
    }
}

ParticipantDiscoveryDatabase::smart_iterator ParticipantDiscoveryDatabase::smart_iterator::operator ++()
{
    do
    {
        ++wrap_it_;
    }
    while ( wrap_it_ != ref_cont_.end() && !wrap_it_->is_alive );

    return *this;
}

ParticipantDiscoveryDatabase::smart_iterator ParticipantDiscoveryDatabase::smart_iterator::operator ++(
        int)
{
    smart_iterator tmp(*this);

    operator ++();

    return tmp;
}

ParticipantDiscoveryDatabase::smart_iterator::reference ParticipantDiscoveryDatabase::smart_iterator::operator *() const
{
    return (reference) * wrap_it_;
}

ParticipantDiscoveryDatabase::smart_iterator::pointer ParticipantDiscoveryDatabase::smart_iterator::operator ->() const
{
    return &(**this);
}

bool ParticipantDiscoveryDatabase::smart_iterator::operator ==(
        const smart_iterator& it) const
{
    // note that zombies are skipped, that simplifies comparison
    return wrap_it_ == it.wrap_it_;
}

bool ParticipantDiscoveryDatabase::smart_iterator::operator !=(
        const smart_iterator& it) const
{
    // note that zombies are skipped, that simplifies comparison
    return !(*this == it);
}

ParticipantDiscoveryDatabase::smart_iterator ParticipantDiscoveryDatabase::smart_iterator::end() const
{
    // the end iterator matches the wrapped iterator one
    smart_iterator tmp(ref_cont_);
    tmp.wrap_it_ = ref_cont_.end();
    return tmp;
}

// acceptable snapshot misalignment in ms
std::chrono::milliseconds Snapshot::acceptable_offset_ = std::chrono::milliseconds(400);

// Time conversion auxiliary
std::chrono::system_clock::time_point Snapshot::_system_clock(std::chrono::system_clock::now());
std::chrono::steady_clock::time_point Snapshot::_steady_clock(std::chrono::steady_clock::now());

std::chrono::system_clock::time_point Snapshot::getSystemTime(
        std::chrono::steady_clock::time_point tp)
{
    using namespace std::chrono;

    return _system_clock + duration_cast<system_clock::duration>(tp - _steady_clock);
}

std::string Snapshot::getTimeStamp(
        std::chrono::steady_clock::time_point snap_time)
{
    std::chrono::system_clock::time_point tp = Snapshot::getSystemTime(snap_time);
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::chrono::milliseconds ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(tp - std::chrono::system_clock::from_time_t(time));

    std::ostringstream stream;
    stream << std::put_time(localtime(&time), "%F %T.") << std::setw(3) << std::setfill('0') << ms.count() << " ";

    return stream.str();
}

// DiscoveryItemDatabase methods

// Lifetime of the return objects is not guaranteed, do not store
std::vector<const ParticipantDiscoveryItem*> DiscoveryItemDatabase::FindParticipant(
        const GUID_t& ptid) const
{
    std::lock_guard<std::mutex> lock(database_mutex);

    std::vector<const ParticipantDiscoveryItem*> v;

    // traverse the map of participants searching for one particular specific info
    for (Snapshot::const_iterator pit = image.cbegin(); pit != image.cend(); ++pit)
    {
        const ParticipantDiscoveryDatabase& _database = *pit;
        auto it = std::lower_bound(_database.cbegin(), _database.cend(), ptid);

        if (it != _database.end() && *it == ptid)
        {
            v.push_back(&*it);
        }
    }

    return v;
}

bool DiscoveryItemDatabase::AddParticipant(
        const GUID_t& spokesman,
        const std::string& srcName,
        const GUID_t& ptid,
        const std::string& name,
        const std::chrono::steady_clock::time_point& discovered_timestamp,
        bool server /* = false*/)
{
    std::lock_guard<std::mutex> lock(database_mutex);

    ParticipantDiscoveryDatabase& _database = image.access_snapshot(spokesman, srcName);
    ParticipantDiscoveryDatabase::iterator it = std::lower_bound(_database.begin(), _database.end(), ptid);

    if (it == _database.end() || *it != ptid)
    {
        // add participant
        it = _database.emplace_hint(it, ptid, name, server);
    }

    // already there, assert liveliness
    if (!it->is_alive)
    {
        // update the zombie
        it->setName(name);
        it->acknowledge(true);
        it->setServer(server);
        it->setDiscoveredTimestamp(discovered_timestamp);
    }

    assert(it->is_server == server);

    return true;

}

bool DiscoveryItemDatabase::RemoveParticipant(
        const GUID_t& deceased)
{
    std::lock_guard<std::mutex> lock(database_mutex);

    return image.erase(deceased) != 0;
}

bool DiscoveryItemDatabase::RemoveParticipant(
        const GUID_t& spokesman,
        const GUID_t& ptid)
{
    std::lock_guard<std::mutex> lock(database_mutex);

    ParticipantDiscoveryDatabase& _database = image[spokesman];
    ParticipantDiscoveryDatabase::iterator it = std::lower_bound(_database.begin(), _database.end(), ptid);

    if (it == _database.end() || *it != ptid)
    {
        return false; // is no there
    }

    // If it isn't empty, mark as dead, otherwise remove
    if (it->CountEndpoints() > 0)
    {
        // participant death acknowledge but not their owned endpoints
        it->acknowledge(false);
    }
    else
    {
        // participant is done
        _database.erase(it);
    }

    return true;
}

template<class T>
bool DiscoveryItemDatabase::AddEndPoint(
        T& (ParticipantDiscoveryItem::* m)() const,
        const GUID_t& spokesman,
        const std::string& srcName,
        const GUID_t& ptid,
        const GUID_t& id,
        const std::string& _typename,
        const std::string& topicname,
        const std::chrono::steady_clock::time_point& discovered_timestamp)
{
    std::lock_guard<std::mutex> lock(database_mutex);

    ParticipantDiscoveryDatabase& _database = image.access_snapshot(spokesman, srcName);
    ParticipantDiscoveryDatabase::iterator it = std::lower_bound(_database.begin(), _database.end(), ptid);

    if (it == _database.end() || ptid == spokesman )
    {
        // participant is no there, add a zombie participant
        it = _database.emplace_hint(it, ptid);

        // participant death acknowledge but not their owned endpoints
        it->acknowledge(ptid == spokesman);
    }

    T& cont = (*it.*m)();
    typename T::iterator sit = std::lower_bound(cont.begin(), cont.end(), id);

    if (sit == cont.end() || *sit != id )
    {
        // add endpoint
        sit = cont.emplace_hint(sit, id, _typename, topicname, discovered_timestamp);
    }

    assert(_typename == sit->type_name);
    assert(topicname == sit->topic_name);

    return true;
}

template<class T>
bool DiscoveryItemDatabase::RemoveEndPoint(
        T& (ParticipantDiscoveryItem::* m)() const,
        const GUID_t& spokesman,
        const GUID_t& ptid,
        const GUID_t& id)
{
    std::lock_guard<std::mutex> lock(database_mutex);

    if (image.find(spokesman) == image.end())
    {
        return false;
    }

    ParticipantDiscoveryDatabase& database = image[spokesman];
    ParticipantDiscoveryDatabase::iterator it = std::lower_bound(database.begin(), database.end(), ptid);

    if (it == database.end() || *it != ptid)
    {
        // participant is not there, should be a zombie
        return false;
    }

    T& cont = (*it.*m)();
    typename T::iterator sit = std::lower_bound(cont.begin(), cont.end(), id);

    if (sit == cont.end() || *sit != id )
    {
        // endpoint is not there
        return false;
    }

    cont.erase(sit);

    if (it->CountEndpoints() == 0 && !it->is_alive)
    {
        // remove participant if zombie
        database.erase(it);
    }
    return true;
}

bool DiscoveryItemDatabase::AddDataReader(
        const GUID_t& spokesman,
        const std::string& srcName,
        const GUID_t& ptid,
        const GUID_t& sid,
        const std::string& _typename,
        const std::string& topicname,
        const std::chrono::steady_clock::time_point& discovered_timestamp)
{
    return AddEndPoint(&ParticipantDiscoveryItem::getDataReaders, spokesman, srcName, ptid, sid, _typename, topicname,
                   discovered_timestamp);
}

bool DiscoveryItemDatabase::RemoveDataReader(
        const GUID_t& spokesman,
        const GUID_t& ptid,
        const GUID_t& sid)
{
    return RemoveEndPoint(&ParticipantDiscoveryItem::getDataReaders, spokesman, ptid, sid);
}

bool DiscoveryItemDatabase::AddDataWriter(
        const GUID_t& spokesman,
        const std::string& srcName,
        const GUID_t& ptid,
        const GUID_t& pid,
        const std::string& _typename,
        const std::string& topicname,
        const std::chrono::steady_clock::time_point& discovered_timestamp)
{
    return AddEndPoint(&ParticipantDiscoveryItem::getDataWriters, spokesman, srcName, ptid, pid, _typename, topicname,
                   discovered_timestamp);
}

bool DiscoveryItemDatabase::RemoveDataWriter(
        const GUID_t& spokesman,
        const GUID_t& ptid,
        const GUID_t& pid)
{
    return RemoveEndPoint(&ParticipantDiscoveryItem::getDataWriters, spokesman, ptid, pid);
}

void DiscoveryItemDatabase::UpdateSubLiveliness(
        const GUID_t& subs,
        int32_t alive_count,
        int32_t not_alive_count)
{
    std::lock_guard<std::mutex> lock(database_mutex);

    // Retrieve the participant that owns this subscriber
    GUID_t pguid(subs);
    pguid.entityId = eprosima::fastdds::rtps::c_EntityId_RTPSParticipant;

    if (image.find(pguid) == image.end())
    {
        return;
    }
    // Locate the PtDI associated with the subscriber
    ParticipantDiscoveryDatabase& database = image[pguid];
    ParticipantDiscoveryDatabase::iterator it = std::lower_bound(database.begin(), database.end(), pguid);

    if (it == database.end() || *it != pguid)
    {
        // participant should be here because the subscriber should create it on its callback
        LOG_ERROR("Non reported subscriber liveliness callback. Participant:" << pguid);
        return;
    }

    // Locate the SDI associated with the subscriber
    ParticipantDiscoveryItem::subscriber_set& ss = it->getDataReaders();
    ParticipantDiscoveryItem::subscriber_set::iterator sit = std::lower_bound(ss.begin(), ss.end(), subs);

    if (sit == ss.end() || *sit != subs)
    {
        // subscriber should be here because should be created on its callback
        LOG_ERROR("Non reported subscriber liveliness callback. Subscriber: " << subs);
        return;
    }

    // Update the liveliness info
    DataReaderDiscoveryItem& sub = const_cast<DataReaderDiscoveryItem&>(*sit);

    sub.alive_count = alive_count;
    sub.not_alive_count = not_alive_count;

    LOG_INFO("Subscriber " << subs << " liveliness callback reporting:"
            " alive_count " << alive_count <<
            " not_alive_count " << not_alive_count )
}

DiscoveryItemDatabase::size_type DiscoveryItemDatabase::CountParticipants(
        const GUID_t& spokesman) const
{
    std::lock_guard<std::mutex> lock(database_mutex);

    const ParticipantDiscoveryDatabase* p = image[spokesman];

    if (p != nullptr)
    {
        return p->size();
    }

    return 0;
}

DiscoveryItemDatabase::size_type DiscoveryItemDatabase::CountDataReaders(
        const GUID_t& spokesman) const
{
    std::lock_guard<std::mutex> lock(database_mutex);

    const ParticipantDiscoveryDatabase* p = image[spokesman];

    if (p != nullptr)
    {
        size_type count = 0;

        for (const ParticipantDiscoveryItem& part : *p)
        {
            count += part.datareaders.size();
        }

        return count;
    }

    return 0;
}

DiscoveryItemDatabase::size_type DiscoveryItemDatabase::CountDataWriters(
        const GUID_t& spokesman) const
{
    std::lock_guard<std::mutex> lock(database_mutex);

    const ParticipantDiscoveryDatabase* p = image[spokesman];

    if (p != nullptr)
    {
        size_type count = 0;

        for (const ParticipantDiscoveryItem& part : *p)
        {
            count += part.datawriters.size();
        }

        return count;
    }

    return 0;
}

DiscoveryItemDatabase::size_type DiscoveryItemDatabase::CountDataReaders(
        const GUID_t& spokesman,
        const GUID_t& ptid) const
{
    std::lock_guard<std::mutex> lock(database_mutex);

    const ParticipantDiscoveryDatabase* p = image[spokesman];

    if (p != nullptr)
    {
        const ParticipantDiscoveryDatabase& database = *p;
        ParticipantDiscoveryDatabase::iterator it = std::lower_bound(database.begin(), database.end(), ptid);

        if (it == database.end() || *it != ptid)
        {
            // participant is no there
            return 0;
        }

        return it->datareaders.size();
    }

    return 0;
}

DiscoveryItemDatabase::size_type DiscoveryItemDatabase::CountDataWriters(
        const GUID_t& spokesman,
        const GUID_t& ptid) const
{
    std::lock_guard<std::mutex> lock(database_mutex);

    const ParticipantDiscoveryDatabase* p = image[spokesman];

    if (p != nullptr)
    {
        const ParticipantDiscoveryDatabase& database = *p;
        ParticipantDiscoveryDatabase::iterator it = std::lower_bound(database.begin(), database.end(), ptid);

        if (it == database.end() || *it != ptid)
        {
            // participant is no there
            return 0;
        }

        return it->datawriters.size();
    }

    return 0;
}

bool eprosima::discovery_server::operator ==(
        const ParticipantDiscoveryDatabase& cl,
        const ParticipantDiscoveryDatabase& cr)
{
    // In order to optimize the algorithm we make l < r. Note that the collections are ordered.
    bool swap = cr.endpoint_guid < cl.endpoint_guid;
    const ParticipantDiscoveryDatabase& l = swap ? cr : cl;
    const ParticipantDiscoveryDatabase& r = swap ? cl : cr;

    // Note that each participant doesn't keep its own discovery info
    // The only acceptable difference between participants discovery information is their own
    // I cannot use direct == on these sets

    // Note that we must ignore the zombie participants (those reported dead but whose endpoints dead is not reported)
    // Thus, we use and special iteration for convenience:

    ParticipantDiscoveryDatabase::smart_iterator lit = l.sbegin(), rit = r.sbegin();
    bool go = true;

    while (go)
    {
        // one of the list reach an end
        if (lit == l.send())
        {
            // finish simultaneously or differ only in
            // each other discovery data
            return (rit == r.send()
                   || ((rit->endpoint_guid == l.endpoint_guid || rit->endpoint_guid == r.endpoint_guid) &&
                   r.send() == ++rit));
        }

        if (rit == r.send())
        {
            // finish simultaneously or differ only in
            // each other discovery data
            return (lit == l.send()
                   || ((lit->endpoint_guid == r.endpoint_guid || lit->endpoint_guid == l.endpoint_guid) &&
                   l.send() == ++lit));
        }

        // comparing elements
        if (lit->endpoint_guid == rit->endpoint_guid)
        {
            // check members
            if (*lit != *rit)
            {
                return false;
            }

            // next iteration
            ++lit;
            ++rit;
        }
        else
        {
            // check if our own discovery data is interfering
            go = false;

            if (rit->endpoint_guid == l.endpoint_guid)
            {
                go = true; // sweep over
                ++rit;
            }

            if (lit->endpoint_guid == r.endpoint_guid
                    && (r.real_size() == l.real_size() || !go ))
            {
                go = true; // sweep over
                ++lit;
            }
        }
    }

    // one or several unknown participants within
    return false;
}

ParticipantDiscoveryDatabase& Snapshot::access_snapshot (
        const GUID_t& id,
        const std::string& name)
{
    auto it = std::lower_bound(begin(), end(), id);
    const ParticipantDiscoveryDatabase* p = nullptr;

    if (it == end() || *it != id)
    {
        // not there, emplace
        p = &(*emplace(id, name).first);
    }
    else
    {
        p = &*it;
    }

    return const_cast<ParticipantDiscoveryDatabase&>(*p);
}

ParticipantDiscoveryDatabase& Snapshot::operator [](
        const GUID_t& id)
{
    auto it = std::lower_bound(begin(), end(), id);
    const ParticipantDiscoveryDatabase* p = nullptr;

    if (it == end() || *it != id)
    {
        // not there, emplace
        // should never be called from this operator
        p = &(*emplace(id, "").first);
    }
    else
    {
        p = &*it;
    }

    return const_cast<ParticipantDiscoveryDatabase&>(*p);
}

const ParticipantDiscoveryDatabase* Snapshot::operator [](
        const GUID_t& id) const
{
    auto it = std::lower_bound(begin(), end(), id);

    if (it == end() || *it != id)
    {
        return nullptr; // not there
    }

    return &*it;
}

void Snapshot::to_xml(
        tinyxml2::XMLElement* pRoot,
        tinyxml2::XMLDocument& xmlDoc) const
{
    using namespace tinyxml2;

    // timestamp time is recorded in ms from the POSIX epoch
    pRoot->SetAttribute(s_sTimestamp.c_str(),
            std::chrono::duration_cast<std::chrono::milliseconds>(getSystemTime(_time).time_since_epoch()).count());

    // process_time is recorded in ms from the process startup
    pRoot->SetAttribute(s_sProcessTime.c_str(),
            std::chrono::duration_cast<std::chrono::milliseconds>(_time - process_startup_).count());

    // last_?dp_callback time is recorded in ms from the process startup
    pRoot->SetAttribute(s_sLastPdpCallback.c_str(),
            std::chrono::duration_cast<std::chrono::milliseconds>(last_PDP_callback_ - process_startup_).count());
    pRoot->SetAttribute(s_sLastEdpCallback.c_str(),
            std::chrono::duration_cast<std::chrono::milliseconds>(last_EDP_callback_ - process_startup_).count());

    pRoot->SetAttribute(s_sSomeone.c_str(), if_someone);

    XMLElement* pDescription = xmlDoc.NewElement(s_sDescription.c_str());
    pDescription->SetText(this->_des.c_str());
    pRoot->InsertEndChild(pDescription);

    for (const ParticipantDiscoveryDatabase& discovery_database : *this)
    {
        XMLElement* pPtdb = xmlDoc.NewElement(s_sPtDB.c_str());
        {
            std::stringstream sstream;
            sstream << discovery_database.endpoint_guid.guidPrefix;
            pPtdb->SetAttribute(s_sGUID_prefix.c_str(), sstream.str().c_str());
        }
        {
            std::stringstream sstream;
            sstream << discovery_database.endpoint_guid.entityId;
            pPtdb->SetAttribute(s_sGUID_entity.c_str(), sstream.str().c_str());
        }
        {
            std::stringstream sstream;
            sstream << discovery_database.participant_name_;
            pPtdb->SetAttribute(s_sName.c_str(), sstream.str().c_str());
        }

        for (const ParticipantDiscoveryItem& discovery_item : discovery_database)
        {
            XMLElement* pPtdi = xmlDoc.NewElement(s_sPtDI.c_str());
            {
                std::stringstream sstream;
                sstream << discovery_item.endpoint_guid.guidPrefix;
                pPtdi->SetAttribute(s_sGUID_prefix.c_str(), sstream.str().c_str());
            }
            {
                std::stringstream sstream;
                sstream << discovery_item.endpoint_guid.entityId;
                pPtdi->SetAttribute(s_sGUID_entity.c_str(), sstream.str().c_str());
            }

            pPtdi->SetAttribute(s_sServer.c_str(), discovery_item.is_server);
            pPtdi->SetAttribute(s_sAlive.c_str(), discovery_item.is_alive);
            pPtdi->SetAttribute(s_sName.c_str(), discovery_item.participant_name.c_str());
            pPtdi->SetAttribute(s_sDiscovered_timestamp.c_str(),
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        discovery_item.discovered_timestamp_ - process_startup_).count());

            for (const DataReaderDiscoveryItem& sub : discovery_item.datareaders)
            {
                XMLElement* pSub = xmlDoc.NewElement(s_sSubscriber.c_str());
                pSub->SetAttribute(s_sType.c_str(), sub.type_name.c_str());
                pSub->SetAttribute(s_sTopic.c_str(), sub.topic_name.c_str());
                {
                    std::stringstream sstream;
                    sstream << sub.endpoint_guid.guidPrefix;
                    pSub->SetAttribute(s_sGUID_prefix.c_str(), sstream.str().c_str());
                }
                {
                    std::stringstream sstream;
                    sstream << sub.endpoint_guid.entityId;
                    pSub->SetAttribute(s_sGUID_entity.c_str(), sstream.str().c_str());
                }
                pSub->SetAttribute(s_sDiscovered_timestamp.c_str(),
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            sub.discovered_timestamp_ - process_startup_).count());

                // Show liveliness callback info if requested, only makes sense to show
                // liveliness on this participant endpoints
                if (show_liveliness_ &&
                        (sub.endpoint_guid.guidPrefix == discovery_database.endpoint_guid.guidPrefix))
                {
                    pSub->SetAttribute(s_sAliveCount.c_str(), sub.alive_count);
                    pSub->SetAttribute(s_sNotAliveCount.c_str(), sub.not_alive_count);
                }

                pPtdi->InsertEndChild(pSub);
            }

            for (const DataWriterDiscoveryItem& pub : discovery_item.datawriters)
            {
                XMLElement* pPub = xmlDoc.NewElement(s_sPublisher.c_str());
                pPub->SetAttribute(s_sType.c_str(), pub.type_name.c_str());
                pPub->SetAttribute(s_sTopic.c_str(), pub.topic_name.c_str());
                {
                    std::stringstream sstream;
                    sstream << pub.endpoint_guid.guidPrefix;
                    pPub->SetAttribute(s_sGUID_prefix.c_str(), sstream.str().c_str());
                }
                {
                    std::stringstream sstream;
                    sstream << pub.endpoint_guid.entityId;
                    pPub->SetAttribute(s_sGUID_entity.c_str(), sstream.str().c_str());
                }
                pPub->SetAttribute(s_sDiscovered_timestamp.c_str(),
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            pub.discovered_timestamp_ - process_startup_).count());
                pPtdi->InsertEndChild(pPub);
            }

            pPtdb->InsertEndChild(pPtdi);
        }

        pRoot->InsertEndChild(pPtdb);
    }
}

void Snapshot::from_xml(
        tinyxml2::XMLElement* pRoot)
{
    using namespace tinyxml2;
    using eprosima::fastdds::rtps::GuidPrefix_t;
    using eprosima::fastdds::rtps::EntityId_t;

    if (pRoot != nullptr)
    {
        {
            // load timestamps
            using namespace std::chrono;

            milliseconds dts(pRoot->Int64Attribute(s_sTimestamp.c_str()));
            milliseconds dpt(pRoot->Int64Attribute(s_sProcessTime.c_str()));
            milliseconds d_pdp_cb(pRoot->Int64Attribute(s_sLastPdpCallback.c_str()));
            milliseconds d_edp_cb(pRoot->Int64Attribute(s_sLastEdpCallback.c_str()));

            // recreate the steady_clock::time_point from the timestamp
            _time = (system_clock::time_point() + dts) - _system_clock + _steady_clock;

            // update the original process startup time for this snapshot
            process_startup_ = _time - dpt;

            // recreate the steady_clock__time_point from last_callback
            last_PDP_callback_ = process_startup_ + d_pdp_cb;
            last_EDP_callback_ = process_startup_ + d_edp_cb;
        }

        if_someone = pRoot->BoolAttribute(s_sSomeone.c_str(), true);

        XMLElement* pDescription = pRoot->FirstChildElement(s_sDescription.c_str());
        if (pDescription != nullptr)
        {
            _des = pDescription->GetText();
            //std::cout << "Description: " << _des << std::endl;
        }

        for (XMLElement* pPtdb = pRoot->FirstChildElement(s_sPtDB.c_str());
                pPtdb != nullptr;
                pPtdb = pPtdb->NextSiblingElement(s_sPtDB.c_str()))
        {
            GUID_t ptdb_guid;
            {
                GuidPrefix_t guidPrefix;
                {
                    std::string guid = pPtdb->Attribute(s_sGUID_prefix.c_str());
                    std::stringstream sstream;
                    sstream << guid;
                    sstream >> guidPrefix;
                }
                EntityId_t entityId;
                {
                    std::string guid = pPtdb->Attribute(s_sGUID_entity.c_str());
                    std::stringstream sstream;
                    sstream << guid;
                    sstream >> entityId;
                }
                ptdb_guid.guidPrefix = guidPrefix;
                ptdb_guid.entityId = entityId;
            }

            ParticipantDiscoveryDatabase discovery_database(ptdb_guid);

            for (XMLElement* pPtdi = pPtdb->FirstChildElement(s_sPtDI.c_str());
                    pPtdi != nullptr;
                    pPtdi = pPtdi->NextSiblingElement(s_sPtDI.c_str()))
            {
                GUID_t ptdi_guid;
                {
                    GuidPrefix_t guidPrefix;
                    {
                        std::string guid = pPtdi->Attribute(s_sGUID_prefix.c_str());
                        std::stringstream sstream;
                        sstream << guid;
                        sstream >> guidPrefix;
                    }
                    EntityId_t entityId;
                    {
                        std::string guid = pPtdi->Attribute(s_sGUID_entity.c_str());
                        std::stringstream sstream;
                        sstream << guid;
                        sstream >> entityId;
                    }
                    ptdi_guid.guidPrefix = guidPrefix;
                    ptdi_guid.entityId = entityId;
                }

                ParticipantDiscoveryItem discovery_item(
                    ptdi_guid,
                    pPtdi->Attribute(s_sName.c_str()),
                    pPtdi->BoolAttribute(s_sServer.c_str()),
                    process_startup_ + std::chrono::milliseconds(pPtdi->Int64Attribute(s_sDiscovered_timestamp.c_str()))
                    );

                pPtdi->QueryBoolAttribute(s_sAlive.c_str(), &discovery_item.is_alive);

                for (XMLElement* pSub = pPtdi->FirstChildElement(s_sSubscriber.c_str());
                        pSub != nullptr;
                        pSub = pSub->NextSiblingElement(s_sSubscriber.c_str()))
                {
                    GUID_t sub_guid;
                    {
                        GuidPrefix_t guidPrefix;
                        {
                            std::string guid = pSub->Attribute(s_sGUID_prefix.c_str());
                            std::stringstream sstream;
                            sstream << guid;
                            sstream >> guidPrefix;
                        }
                        EntityId_t entityId;
                        {
                            std::string guid = pSub->Attribute(s_sGUID_entity.c_str());
                            std::stringstream sstream;
                            sstream << guid;
                            sstream >> entityId;
                        }
                        sub_guid.guidPrefix = guidPrefix;
                        sub_guid.entityId = entityId;

                    }

                    std::chrono::milliseconds disc_t(pSub->Int64Attribute(s_sDiscovered_timestamp.c_str()));
                    DataReaderDiscoveryItem sub(sub_guid, pSub->Attribute(s_sType.c_str()),
                            pSub->Attribute(s_sTopic.c_str()),
                            process_startup_ + disc_t);

                    // retrieve liveliness values if any
                    if ((XML_NO_ATTRIBUTE != pSub->QueryAttribute(s_sAliveCount.c_str(), &sub.alive_count)) ||
                            (XML_NO_ATTRIBUTE != pSub->QueryAttribute(s_sNotAliveCount.c_str(), &sub.not_alive_count)))
                    {
                        show_liveliness_ = true; // if present any attributes set liveliness
                    }

                    discovery_item.datareaders.insert(std::move(sub));
                }

                for (XMLElement* pPub = pPtdi->FirstChildElement(s_sPublisher.c_str());
                        pPub != nullptr;
                        pPub = pPub->NextSiblingElement(s_sPublisher.c_str()))
                {
                    GUID_t pub_guid;
                    {
                        GuidPrefix_t guidPrefix;
                        {
                            std::string guid = pPub->Attribute(s_sGUID_prefix.c_str());
                            std::stringstream sstream;
                            sstream << guid;
                            sstream >> guidPrefix;
                        }
                        EntityId_t entityId;
                        {
                            std::string guid = pPub->Attribute(s_sGUID_entity.c_str());
                            std::stringstream sstream;
                            sstream << guid;
                            sstream >> entityId;
                        }
                        pub_guid.guidPrefix = guidPrefix;
                        pub_guid.entityId = entityId;
                    }

                    std::chrono::milliseconds disc_t(pPub->Int64Attribute(s_sDiscovered_timestamp.c_str()));
                    DataWriterDiscoveryItem pub(pub_guid, pPub->Attribute(s_sType.c_str()),
                            pPub->Attribute(s_sTopic.c_str()),
                            process_startup_ + disc_t);
                    discovery_item.datawriters.insert(std::move(pub));
                }

                discovery_database.insert(std::move(discovery_item));
            }

            this->insert(std::move(discovery_database));

        }
    }
}

Snapshot& Snapshot::operator +=(
        const Snapshot& sh)
{
    // Verify snapshot sync
    std::chrono::milliseconds offset =
            std::chrono::duration_cast<std::chrono::milliseconds>(_time - sh._time);

    // if( abs(offset) > Snapshot::acceptable_offset_ ) // uses abs(duration< ...) which is a C++17 hack
    if ( abs(offset.count()) > Snapshot::acceptable_offset_.count())
    {
        LOG_ERROR("Watch out Snapshot sync. They are " << abs(offset.count()) << " ms away.");
    }

    // We keep the later last call in the merging
    if ((last_PDP_callback_ - process_startup_) < (sh.last_PDP_callback_ - sh.process_startup_))
    {
        last_PDP_callback_ = sh.last_PDP_callback_;
    }

    if ((last_EDP_callback_ - process_startup_) < (sh.last_EDP_callback_ -  sh.process_startup_))
    {
        last_EDP_callback_ = sh.last_EDP_callback_;
    }

    insert(sh.begin(), sh.end());

    // flag philosophy: a single non default value in a config drives all.
    // OR the liveliness, if any config file requires liveliness the global output shows it
    show_liveliness_ |= sh.show_liveliness_;
    // AND valid if empty, if any config file allows empty snapshots the global allows it
    if_someone &= sh.if_someone;

    return *this;
}

std::ostream& eprosima::discovery_server::operator <<(
        std::ostream& os,
        const Snapshot& shot)
{
    using namespace std;
    using namespace std::chrono;

    os << "Snapshot taken at " << Snapshot::getTimeStamp(shot._time) << "or ";
    os << duration_cast<milliseconds>(shot._time - shot.process_startup_).count();
    os << " ms since process startup." << " Description: " << shot._des << std::endl;

    os << "Snapshot process startup at " << Snapshot::getTimeStamp(shot.process_startup_) << endl;

    os << "Last PDP callback at " << Snapshot::getTimeStamp(shot.last_PDP_callback_) << "or ";
    os << duration_cast<milliseconds>(shot.last_PDP_callback_ - shot.process_startup_).count();
    os << " ms since process startup." << endl;

    os << "Last EDP callback at " << Snapshot::getTimeStamp(shot.last_EDP_callback_) << "or ";
    os << duration_cast<milliseconds>(shot.last_EDP_callback_ - shot.process_startup_).count();
    os << " ms since process startup." << endl;

    os << shot.size() << " participants report the following discovery info:" << endl;

    for (const ParticipantDiscoveryDatabase& db : shot)
    {
        os << db << endl;
    }

    return os;
}
