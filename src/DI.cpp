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
#include <iterator>
#include <sstream>
#include <iomanip>
#include <tinyxml2.h>
#include "DI.h"

#ifndef XMLCheckResult
	#define XMLCheckResult(a_eResult) if (a_eResult != XML_SUCCESS) { printf("Error: %i\n", a_eResult); \
    return a_eResult; }
#endif

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

bool DI::operator!=(const DI & d) const
{
    return _id != d._id;
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
    return DI::operator==(p)
        && _typeName == p._typeName
        && _topicName == p._topicName;
}

std::ostream& eprosima::discovery_server::operator<<(std::ostream& os, const PDI& di)
{
    return os << "Publisher " << di._id << " TypeName: " << di._typeName
        << " TopicName: " << di._topicName;
}

// subscriber discovery item operations
bool SDI::operator==(const SDI & p) const
{
    return DI::operator==(p)
        && _typeName == p._typeName
        && _topicName == p._topicName;
}

std::ostream& eprosima::discovery_server::operator<<(std::ostream& os, const SDI& di)
{
    return os << "Subscriber " << di._id << " TypeName: " << di._typeName
        << " TopicName: " << di._topicName;
}

// participant discovery item operations

bool PtDI::operator==(const PtDI & p) const
{
    return DI::operator==(p)
        // && this->_alive == p._alive // own participant may not be aware
        // && this->_server == p._server // only in-process participants may be aware of this
        // && this->_name == p._name // own participant may not be aware
        && this->_publishers == p._publishers
        && this->_subscribers == p._subscribers;
}

bool PtDI::operator!=(const PtDI & p) const
{
    return DI::operator!=(p)
        // || this->_alive != p._alive // own participant may not be aware
        // || this->_server != p._server // only in-process participants may be aware of this
        // || this->_name != p._name // own participant may not be aware
        || this->_publishers != p._publishers
        || this->_subscribers != p._subscribers;
}

std::ostream& eprosima::discovery_server::operator<<(std::ostream& os, const PtDI& di)
{
    os << "\t Participant ";

    if (!di._name.empty())
    {
        os << di._name << ' ';
    }

    os << di._id;

    if ( di.CountEndpoints() > 0 )
    {
        os << " has:" << std::endl;
    }

    if (di._publishers.size())
    {
        os << "\t\t" << di._publishers.size() << " publishers:" << std::endl;

        for ( const PDI & pdi : di._publishers )
        {
            os << "\t\t\t" << pdi << std::endl;
        }
    }

    if (di._subscribers.size())
    {
        os << "\t\t" << di._subscribers.size() << " subscribers:" << std::endl;

        for (const SDI & sdi : di._subscribers)
        {
            os << "\t\t\t" << sdi << std::endl;
        }
    }

    return os;
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

std::ostream& eprosima::discovery_server::operator<<(std::ostream& os, const PtDB& db)
{
    os << "Participant " << db._id << " discovered: " << std::endl;

    for (const PtDI & pt : db)
    {
        os << pt << std::endl;
    }

    return os;
}

// Time conversion auxiliary
std::chrono::system_clock::time_point Snapshot::_sy_ck(std::chrono::system_clock::now());
std::chrono::steady_clock::time_point Snapshot::_st_ck(std::chrono::steady_clock::now());

std::time_t Snapshot::getSystemTime() const
{
    using namespace std::chrono;

    return system_clock::to_time_t(_sy_ck + duration_cast<system_clock::duration>(_time - _st_ck));
}

// DI_database methods

// livetime of the return objects is not guaranteed, do not store
std::vector<const PtDI*> DI_database::FindParticipant(const GUID_t & ptid) const
{
    std::lock_guard<std::mutex> lock(_mtx);

    std::vector<const PtDI*> v;

    // traverse the map of participants searching for one particular specific info
    for (Snapshot::const_iterator pit = _participants.cbegin(); pit != _participants.cend(); ++pit)
    {
        const PtDB & _database = *pit;
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

    PtDB & _database = _participants[spokesman];
    PtDB::iterator it = std::lower_bound(_database.begin(), _database.end(), ptid);

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

bool DI_database::RemoveParticipant(const GUID_t& deceased)
{
    std::lock_guard<std::mutex> lock(_mtx);

    return _participants.erase(deceased) != 0;
}

bool DI_database::RemoveParticipant(const GUID_t& spokesman, const GUID_t & ptid)
{
    std::lock_guard<std::mutex> lock(_mtx);

    PtDB & _database = _participants[spokesman];
    PtDB::iterator it = std::lower_bound(_database.begin(), _database.end(),ptid);

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

    PtDB & _database = _participants[spokesman];
    PtDB::iterator it = std::lower_bound(_database.begin(), _database.end(), ptid);

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

    T& cont = (*it.*m)();
    typename T::iterator sit = std::lower_bound(cont.begin(), cont.end(), id);

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

    PtDB & _database = _participants[spokesman];
    PtDB::iterator it = std::lower_bound(_database.begin(), _database.end(), ptid);

    if (it == _database.end() || *it != ptid)
    {
        // participant is no there, should be a zombie
        return false;
    }

    T & cont = (*it.*m)();
    typename T::iterator sit = std::lower_bound(cont.begin(), cont.end(), id);

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

    auto p = _participants[spokesman];

    if (p)
    {
       return p->size();
    }

    return 0;
}

DI_database::size_type DI_database::CountSubscribers(const GUID_t& spokesman) const
{
    std::lock_guard<std::mutex> lock(_mtx);

    auto p = _participants[spokesman];

    if (p)
    {
        size_type count = 0;

        for (auto part : *p)
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

    auto p = _participants[spokesman];

    if (p)
    {
        size_type count = 0;

        for (auto part : *p)
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

    auto p = _participants[spokesman];

    if (p)
    {
        const PtDB & _database = *p;
        PtDB::iterator it = std::lower_bound(_database.begin(), _database.end(), ptid);

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

    auto p = _participants[spokesman];

    if (p)
    {
        const PtDB & _database = *p;
        PtDB::iterator it = std::lower_bound(_database.begin(), _database.end(), ptid);

        if (it == _database.end() || *it != ptid)
        {
            // participant is no there
            return 0;
        }

        return it->_publishers.size();
    }

    return 0;
}

// TODO: test this operator
bool eprosima::discovery_server::operator==(const PtDB & l,const PtDB & r)
{
    // Note that each participant doesn't keep its own discovery info
    // The only acceptable difference between participants discovery information is their own
    // I cannot use direct == on these sets

    PtDB::const_iterator lit = l.begin(), rit = r.begin();
    bool go = true;

    while(go)
    {
        // one of the list reach an end
        if (lit == l.end())
        {
            // finish simultaneously or differ only in
            // each other discovery data
            return (rit == r.end()
                || (rit->_id == r._id && r.end() == ++rit));
        }

        if (rit == r.end())
        {
            // finish simultaneously or differ only in
            // each other discovery data
            return (lit == l.end()
                || (lit->_id == l._id && l.end() == ++lit));
        }

        // comparing elements
        if (lit->_id == rit->_id)
        {
            // check members
            if (*lit != *rit)
                return false;

            // next iteration
            ++lit;
            ++rit;
        }
        else
        {   // check if our own discovery data is interfering
            go = false;

            if (lit++->_id == r._id)
            {
                go = true; // sweep over
            }

            if (rit++->_id == l._id)
            {
                go = true; // sweep over
            }
        }

    };

    // one or several unknown participants within
    return false;
}

PtDB & Snapshot::operator[](const GUID_t & id)
{
    auto it = std::lower_bound(begin(), end(), id);
    const PtDB * p = nullptr;

    if (it == end() || *it != id)
    {  // no there, emplace
         p = &(*emplace(id).first);
    }
    else
    {
        p = &*it;
    }

    return const_cast<PtDB&>(*p);
}

const PtDB * Snapshot::operator[](const GUID_t & id) const
{
    auto it = std::lower_bound(begin(), end(), id);

    if (it == end() || *it != id)
    {
        return nullptr; // no there
    }

    return &*it;
}

void Snapshot::to_xml(tinyxml2::XMLNode* pRoot) const
{
    using namespace tinyxml2;

    XMLDocument xmlDoc;
    XMLElement* pMyRoot = xmlDoc.NewElement("DS_Snapshot");
    pRoot->InsertEndChild(pMyRoot);

    XMLElement* pTimestamp = xmlDoc.NewElement("timestamp");
    pTimestamp->SetText(this->_time.time_since_epoch().count());
    pRoot->InsertEndChild(pTimestamp);

    XMLElement* pDescription = xmlDoc.NewElement("description");
    pDescription->SetText(this->_des.c_str());
    pRoot->InsertEndChild(pDescription);

    for (const PtDB& ptdb : *this)
    {
        XMLElement* pPtdb = xmlDoc.NewElement("ptdb");
        {
            std::stringstream sstream;
            sstream << ptdb._id.guidPrefix;
            pPtdb->SetAttribute("guid_prefix", sstream.str().c_str());
        }
        {
            std::stringstream sstream;
            sstream << ptdb._id.entityId;
            pPtdb->SetAttribute("guid_entity", sstream.str().c_str());
        }

        for (const PtDI& ptdi : ptdb)
        {
            /*
            // identity
            bool _server; // false -> client
            bool _alive; // false if death already reported but owned endpoints yet to be
            std::string _name;

            // local user entities
            publisher_set _publishers;
            subscriber_set _subscribers;
            */
            XMLElement* pPtdi = xmlDoc.NewElement("ptdi");
            {
                std::stringstream sstream;
                sstream << ptdi._id.guidPrefix;
                pPtdi->SetAttribute("guid_prefix",sstream.str().c_str());
            }
            {
                std::stringstream sstream;
                sstream << ptdi._id.entityId;
                pPtdi->SetAttribute("guid_entity",sstream.str().c_str());
            }

            pPtdi->SetAttribute("server", ptdi._server);
            pPtdi->SetAttribute("alive", ptdi._alive);
            pPtdi->SetAttribute("name", ptdi._name.c_str());

            for (const SDI& sub : ptdi._subscribers)
            {
                XMLElement* pSub = xmlDoc.NewElement("subscriber");
                pSub->SetAttribute("type", sub._typeName.c_str());
                pSub->SetAttribute("topic", sub._topicName.c_str());
                {
                    std::stringstream sstream;
                    sstream << sub._id.guidPrefix;
                    pSub->SetAttribute("guid_prefix",sstream.str().c_str());
                }
                {
                    std::stringstream sstream;
                    sstream << sub._id.entityId;
                    pSub->SetAttribute("guid_entity",sstream.str().c_str());
                }
                pPtdi->InsertEndChild(pSub);
            }

            for (const PDI& pub : ptdi._publishers)
            {
                XMLElement* pPub = xmlDoc.NewElement("publisher");
                pPub->SetAttribute("type", pub._typeName.c_str());
                pPub->SetAttribute("topic", pub._topicName.c_str());
                {
                    std::stringstream sstream;
                    sstream << pub._id.guidPrefix;
                    pPub->SetAttribute("guid_prefix",sstream.str().c_str());
                }
                {
                    std::stringstream sstream;
                    sstream << pub._id.entityId;
                    pPub->SetAttribute("guid_entity",sstream.str().c_str());
                }
                pPtdi->InsertEndChild(pPub);
            }

            pPtdb->InsertEndChild(pPtdi);
        }

        pRoot->InsertEndChild(pPtdb);
    }
}

void Snapshot::from_xml(tinyxml2::XMLNode* pRoot)
{
    using namespace tinyxml2;
    using eprosima::fastrtps::rtps::GuidPrefix_t;
    using eprosima::fastrtps::rtps::EntityId_t;

    if (pRoot != nullptr)
    {
        XMLElement* pTimestamp = pRoot->FirstChildElement("timestamp");
        if (pTimestamp != nullptr)
        {
            int64_t time;
            pTimestamp->QueryInt64Text(&time);
            using Ns = std::chrono::nanoseconds;

            this->_time = std::chrono::steady_clock::time_point(Ns(time));
        }

        XMLElement* pDescription = pRoot->FirstChildElement("description");
        if (pDescription != nullptr)
        {
            _des = pDescription->GetText();
        }

        for (XMLElement* pPtdb = pRoot->FirstChildElement("ptdb");
                pPtdb != nullptr;
                pPtdb = pPtdb->NextSiblingElement("ptdb"))
        {
            GUID_t ptdb_guid;
            {
                GuidPrefix_t guidPrefix;
                {
                    std::string guid = pPtdb->Attribute("guid_prefix");
                    std::stringstream sstream;
                    sstream << guid;
                    sstream >> guidPrefix;
                }
                EntityId_t entityId;
                {
                    std::string guid = pPtdb->Attribute("guid_entity");
                    std::stringstream sstream;
                    sstream << guid;
                    sstream >> entityId;
                }
                ptdb_guid.guidPrefix = guidPrefix;
                ptdb_guid.entityId = entityId;
            }
            PtDB ptdb(std::move(ptdb_guid));

            for (XMLElement* pPtdi = pPtdb->FirstChildElement("ptdi");
                    pPtdi != nullptr;
                    pPtdi = pPtdi->NextSiblingElement("ptdi"))
            {
                GUID_t ptdi_guid;
                {
                    GuidPrefix_t guidPrefix;
                    {
                        std::string guid = pPtdb->Attribute("guid_prefix");
                        std::stringstream sstream;
                        sstream << guid;
                        sstream >> guidPrefix;
                    }
                    EntityId_t entityId;
                    {
                        std::string guid = pPtdb->Attribute("guid_entity");
                        std::stringstream sstream;
                        sstream << guid;
                        sstream >> entityId;
                    }
                    ptdi_guid.guidPrefix = guidPrefix;
                    ptdi_guid.entityId = entityId;
                }
                PtDI ptdi(std::move(ptdi_guid));

                pPtdi->QueryBoolAttribute("server", &ptdi._server);
                pPtdi->QueryBoolAttribute("alive", &ptdi._alive);
                ptdi._name = pPtdi->Attribute("name");

                for (XMLElement* pSub = pPtdi->FirstChildElement("subscriber");
                        pSub != nullptr;
                        pSub = pSub->NextSiblingElement("subscriber"))
                {
                    GUID_t sub_guid;
                    {
                        GuidPrefix_t guidPrefix;
                        {
                            std::string guid = pPtdb->Attribute("guid_prefix");
                            std::stringstream sstream;
                            sstream << guid;
                            sstream >> guidPrefix;
                        }
                        EntityId_t entityId;
                        {
                            std::string guid = pPtdb->Attribute("guid_entity");
                            std::stringstream sstream;
                            sstream << guid;
                            sstream >> entityId;
                        }
                        sub_guid.guidPrefix = guidPrefix;
                        sub_guid.entityId = entityId;
                    }
                    SDI sub(std::move(sub_guid), pSub->Attribute("type"), pSub->Attribute("topic"));
                    ptdi._subscribers.emplace(sub);
                }

                for (XMLElement* pPub = pPtdi->FirstChildElement("publisher");
                        pPub != nullptr;
                        pPub = pPub->NextSiblingElement("publisher"))
                {
                    GUID_t pub_guid;
                    {
                        GuidPrefix_t guidPrefix;
                        {
                            std::string guid = pPtdb->Attribute("guid_prefix");
                            std::stringstream sstream;
                            sstream << guid;
                            sstream >> guidPrefix;
                        }
                        EntityId_t entityId;
                        {
                            std::string guid = pPtdb->Attribute("guid_entity");
                            std::stringstream sstream;
                            sstream << guid;
                            sstream >> entityId;
                        }
                        pub_guid.guidPrefix = guidPrefix;
                        pub_guid.entityId = entityId;
                    }
                    PDI pub(std::move(pub_guid), pPub->Attribute("type"), pPub->Attribute("topic"));
                    ptdi._publishers.emplace(pub);
                }

                ptdb.emplace(ptdi);
            }

            this->emplace(ptdb);
        }
    }/*
    else
    {
        LOG_ERROR("Error loading XML file " << file);
    }*/
}

std::ostream& eprosima::discovery_server::operator<<(std::ostream& os, const Snapshot& shot)
{
    std::string timestamp;

    {
        std::time_t time = shot.getSystemTime();
        std::ostringstream stream;

#if defined(_WIN32)
        struct tm timeinfo;
        localtime_s(&timeinfo, &time);
        stream << std::put_time(&timeinfo, "%F %T");
#else
        stream << std::put_time(localtime(&time), "%F %T");
#endif
        timestamp = stream.str();
    }

    os << "Snapshot taken at " << timestamp << " description: " << shot._des << std::endl;
    os << shot.size() << " participants report the following discovery info:" << std::endl;

    for (const PtDB& db : shot)
    {
        os << db << std::endl;
    }

    return os;
}
