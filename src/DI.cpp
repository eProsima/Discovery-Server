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

#include <cassert>
#include <algorithm>
#include "DI.h"

using namespace eprosima::discovery_server;

// basic discovery items operations

bool DI::operator==(const GUID_t & guid) const
{
    return _id == guid;
}

bool DI::operator!=(const GUID_t & guid) const
{
    return _id != guid;
}

bool DI::operator==(const DI & d) const
{
    return _id == d._id;
}

bool DI::operator<(const GUID_t & guid) const
{
    return _id < guid;
}

bool DI::operator<(const DI & d) const
{
    return _id < d._id;
}

// publiser discovery item operations
bool PDI::operator==(const PDI & p) const
{
    assert(_typeName == p._typeName);
    assert(_topicName == p._topicName);

    return DI::operator==(p);
}

// subscriber discovery item operations
bool SDI::operator==(const SDI & p) const
{
    assert(_typeName == p._typeName);
    assert(_topicName == p._topicName);

    return DI::operator==(p);
}

// participant discovery item operations

bool PtDI::operator==(const PtDI &) const
{
    // this is an ugly one to implement
    return true;
}

bool PtDI::operator[](const PDI & p) const
{
    // search the list
    return _publishers.end() != _publishers.find(p);
}

bool PtDI::operator[](const SDI & p) const
{
    // search the list
    return _subscribers.end() != _subscribers.find(p);
}

void PtDI::acknowledge(bool alive) const
{
    // STL makes iterator const to prevent that any key changing unsorts the container
    // so we introduce this method to avoid constant ugly const_cast use
    PtDI & part = const_cast<PtDI &>(*this);
    part._alive = alive;
}

PtDI::size_type PtDI::CountEndpoints() const
{
    return  _publishers.size() + _subscribers.size();
}

// DI_database methods

// livetime of the return objects is not guaranteed, do not store
std::vector<const PtDI*> DI_database::FindParticipant(const GUID_t & ptid) const
{
    std::lock_guard<std::mutex> lock(_mtx);

    std::vector<const PtDI*> v;

    // traverse the map of participants searching for one particular specific info
    for (participant_list::const_iterator pit = _participants.cbegin(); pit != _participants.cend(); ++pit)
    {
        const database & _database = pit->second;
        auto  it = std::lower_bound(_database.cbegin(), _database.cend(), ptid);

        if (it != _database.end() && *it == ptid)
        {
            v.push_back(&*it);
        }
    }

    return v;
}

bool DI_database::AddParticipant(const GUID_t& spokesman, const GUID_t& ptid, const std::string& name, bool server/* = false*/)
{
    std::lock_guard<std::mutex> lock(_mtx);

    database & _database = _participants[spokesman];
    database::iterator it = std::lower_bound(_database.begin(), _database.end(), ptid);

    if (it == _database.end() || *it != ptid)
    { // add participant
        it = _database.emplace_hint(it, ptid, name, server);
    }

    // already there, assert liveliness
    if (!it->_alive)
    {   // update the zombie
        it->setName(name);
        it->acknowledge(true);
        it->setServer(server);
    }

    assert(it->_server == server); 

    return true;

}

bool DI_database::RemoveParticipant(const GUID_t& spokesman, const GUID_t & ptid)
{
    std::lock_guard<std::mutex> lock(_mtx);

    database & _database = _participants[spokesman];
    database::iterator it = std::lower_bound(_database.begin(), _database.end(),ptid);

    if (it == _database.end() || *it != ptid)
    {
        return false; // is no there
    }

    // If isn't empty, mark as death, otherwise remove
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
bool DI_database::AddEndPoint(T&(PtDI::* m)() const, const GUID_t& spokesman, const GUID_t & ptid, const GUID_t & id,
    const std::string & _typename, const std::string & topicname)
{
    std::lock_guard<std::mutex> lock(_mtx);

    database & _database = _participants[spokesman];
    database::iterator it = std::lower_bound(_database.begin(), _database.end(), ptid);

    if (it == _database.end() || *it != ptid )
    {
        // participant is no there, add a zombie participant
        it = _database.emplace_hint(it,ptid);

        // participant death acknowledge but not their owned endpoints
        it->acknowledge(false);
    }

    // STL makes iterator const to prevent that any key changing unsorts the container
    //const PtDI * p = &(*it);
    //PtDI::subscriber_set& ( PtDI::* gS)() const = &PtDI::getSubscribers;
    //PtDI::subscriber_set & subs = (p->*gS)();

    T & cont = (*it.*m)();
    T::iterator sit = std::lower_bound(cont.begin(), cont.end(), id);

    if (sit == cont.end() || *sit != id )
    {
        // add endpoint
        sit = cont.emplace_hint(sit, id, _typename, topicname);
    }

    assert(_typename == sit->_typeName);
    assert(topicname == sit->_topicName);

    return true;
}

template<class T>
bool DI_database::RemoveEndPoint(T&(PtDI::* m)() const, const GUID_t& spokesman, const GUID_t & ptid, const GUID_t & id)
{
    std::lock_guard<std::mutex> lock(_mtx);

    database & _database = _participants[spokesman];
    database::iterator it = std::lower_bound(_database.begin(), _database.end(), ptid);

    if (it == _database.end() || *it != ptid)
    {
        // participant is no there, should be a zombie
        return false;
    }

    T & cont = (*it.*m)();
    T::iterator sit = std::lower_bound(cont.begin(), cont.end(), id);

    if (sit == cont.end() || *sit != id )
    {
        // endpoint is not there
        return false;
    }

    cont.erase(sit);

    if (it->CountEndpoints() == 0 && !it->_alive)
    {
        // remove participant if zombie
        _database.erase(it);
    }

    return true;

}

bool DI_database::AddSubscriber(const GUID_t& spokesman, const GUID_t & ptid, const GUID_t & sid,
    const std::string & _typename, const std::string & topicname)
{
    return AddEndPoint(&PtDI::getSubscribers, spokesman,ptid, sid, _typename, topicname);
}

bool DI_database::RemoveSubscriber(const GUID_t& spokesman, const GUID_t & ptid, const GUID_t & sid)
{
    return RemoveEndPoint(&PtDI::getSubscribers, spokesman, ptid, sid);
}

bool DI_database::AddPublisher(const GUID_t& spokesman, const GUID_t & ptid, const GUID_t & pid, const std::string & _typename, const std::string & topicname)
{
    return AddEndPoint(&PtDI::getPublishers, spokesman, ptid, pid, _typename, topicname);
}

bool DI_database::RemovePublisher(const GUID_t& spokesman, const GUID_t & ptid, const GUID_t & pid)
{
    return RemoveEndPoint(&PtDI::getPublishers, spokesman, ptid, pid);
}

DI_database::size_type DI_database::CountParticipants(const GUID_t& spokesman) const
{
    std::lock_guard<std::mutex> lock(_mtx);

    auto it = _participants.find(spokesman);

    if ( it != _participants.end())
    {
       return it->second.size();
    }

    return 0;
}

DI_database::size_type DI_database::CountSubscribers(const GUID_t& spokesman) const
{
    std::lock_guard<std::mutex> lock(_mtx);

    auto it = _participants.find(spokesman);

    if (it != _participants.end())
    {
        size_type count = 0;

        for (auto part : it->second)
        {
            count += part._subscribers.size();
        }

        return count;
    }

    return 0;
}

DI_database::size_type DI_database::CountPublishers(const GUID_t& spokesman) const
{
    std::lock_guard<std::mutex> lock(_mtx);

    auto it = _participants.find(spokesman);

    if (it != _participants.end())
    {
        size_type count = 0;

        for (auto part : it->second)
        {
            count += part._publishers.size();
        }

        return count;
    }

    return 0;
}

DI_database::size_type DI_database::CountSubscribers(const GUID_t& spokesman,const GUID_t & ptid) const
{
    std::lock_guard<std::mutex> lock(_mtx);

    auto it = _participants.find(spokesman);

    if (it != _participants.end())
    {
        const database & _database = it->second;
        database::iterator it = std::lower_bound(_database.begin(), _database.end(), ptid);

        if (it == _database.end() || *it != ptid)
        {
            // participant is no there
            return 0;
        }

        return it->_subscribers.size();
    }

    return 0;
}

DI_database::size_type DI_database::CountPublishers(const GUID_t& spokesman,const GUID_t & ptid) const
{
    std::lock_guard<std::mutex> lock(_mtx);

    auto it = _participants.find(spokesman);

    if (it != _participants.end())
    {
        const database & _database = it->second;
        database::iterator it = std::lower_bound(_database.begin(), _database.end(), ptid);

        if (it == _database.end() || *it != ptid)
        {
            // participant is no there
            return 0;
        }

        return it->_publishers.size();
    }

    return 0;
}