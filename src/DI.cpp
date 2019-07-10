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

#include "log/DSLog.h"
#include <cassert>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <iomanip>
#include <tinyxml2.h>
#include "DI.h"
#include "IDs.h"

#ifndef XMLCheckResult
	#define XMLCheckResult(a_eResult) if (a_eResult != XML_SUCCESS) { printf("Error: %i\n", a_eResult); \
    return a_eResult; }
#endif

using namespace eprosima::discovery_server;

// basic discovery items operations

bool DI::operator==(const GUID_t& guid) const
{
    return endpoint_guid == guid;
}

bool DI::operator!=(const GUID_t& guid) const
{
    return endpoint_guid != guid;
}

bool DI::operator==(const DI& d) const
{
    return endpoint_guid == d.endpoint_guid;
}

bool DI::operator!=(const DI& d) const
{
    return endpoint_guid != d.endpoint_guid;
}

bool DI::operator<(const GUID_t& guid) const
{
    return endpoint_guid < guid;
}

bool DI::operator<(const DI& d) const
{
    return endpoint_guid < d.endpoint_guid;
}

// publiser discovery item operations
bool PDI::operator==(const PDI& p) const
{
    return DI::operator==(p)
        && type_name == p.type_name
        && topic_name == p.topic_name;
}

std::ostream& eprosima::discovery_server::operator<<(std::ostream& os, const PDI& di)
{
    return os << "Publisher " << di.endpoint_guid << " TypeName: " << di.type_name
        << " TopicName: " << di.topic_name;
}

// subscriber discovery item operations
bool SDI::operator==(const SDI& p) const
{
    return DI::operator==(p)
        && type_name == p.type_name
        && topic_name == p.topic_name;
}

std::ostream& eprosima::discovery_server::operator<<(std::ostream& os, const SDI& di)
{
    return os << "Subscriber " << di.endpoint_guid << " TypeName: " << di.type_name
        << " TopicName: " << di.topic_name;
}

// participant discovery item operations

bool PtDI::operator==(const PtDI& p) const
{
    return DI::operator==(p)
        // && this->is_alive == p.is_alive // own participant may not be aware
        // && this->is_server == p.is_server // only in-process participants may be aware of this
        // && this->participant_name == p.participant_name // own participant may not be aware
        && this->publishers == p.publishers
        && this->subscribers == p.subscribers;
}

bool PtDI::operator!=(const PtDI& p) const
{
    return DI::operator!=(p)
        // || this->is_alive != p.is_alive // own participant may not be aware
        // || this->is_server != p.is_server // only in-process participants may be aware of this
        // || this->participant_name != p.participant_name // own participant may not be aware
        || this->publishers != p.publishers
        || this->subscribers != p.subscribers;
}

std::ostream& eprosima::discovery_server::operator<<(std::ostream& os, const PtDI& di)
{
    os << "\t Participant ";

    if (!di.participant_name.empty())
    {
        os << di.participant_name << ' ';
    }

    os << di.endpoint_guid;

    if ( di.CountEndpoints() > 0 )
    {
        os << " has:" << std::endl;
    }

    if (di.publishers.size())
    {
        os << "\t\t" << di.publishers.size() << " publishers:" << std::endl;

        for ( const PDI & pdi : di.publishers )
        {
            os << "\t\t\t" << pdi << std::endl;
        }
    }

    if (di.subscribers.size())
    {
        os << "\t\t" << di.subscribers.size() << " subscribers:" << std::endl;

        for (const SDI & sdi : di.subscribers)
        {
            os << "\t\t\t" << sdi << std::endl;
        }
    }

    return os;
}

bool PtDI::operator[](const PDI& p) const
{
    // search the list
    return publishers.end() != publishers.find(p);
}

bool PtDI::operator[](const SDI& p) const
{
    // search the list
    return subscribers.end() != subscribers.find(p);
}

void PtDI::acknowledge(bool alive) const
{
    // STL makes iterator const to prevent that any key changing unsorts the container
    // so we introduce this method to avoid constant ugly const_cast use
    PtDI & part = const_cast<PtDI&>(*this);
    part.is_alive = alive;
}

PtDI::size_type PtDI::CountEndpoints() const
{
    return  publishers.size() + subscribers.size();
}

std::ostream& eprosima::discovery_server::operator<<(std::ostream& os, const PtDB& db)
{
    os << "Participant " << db.endpoint_guid << " discovered: " << std::endl;

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
std::vector<const PtDI*> DI_database::FindParticipant(const GUID_t& ptid) const
{
    std::lock_guard<std::mutex> lock(database_mutex);

    std::vector<const PtDI*> v;

    // traverse the map of participants searching for one particular specific info
    for (Snapshot::const_iterator pit = image.cbegin(); pit != image.cend(); ++pit)
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

bool DI_database::AddParticipant(
    const GUID_t& spokesman,
    const GUID_t& ptid,
    const std::string& name,
    bool server/* = false*/)
{
    std::lock_guard<std::mutex> lock(database_mutex);

    PtDB & _database = image[spokesman];
    PtDB::iterator it = std::lower_bound(_database.begin(), _database.end(), ptid);

    if (it == _database.end() || *it != ptid)
    { // add participant
        it = _database.emplace_hint(it, ptid, name, server);
    }

    // already there, assert liveliness
    if (!it->is_alive)
    {   // update the zombie
        it->setName(name);
        it->acknowledge(true);
        it->setServer(server);
    }

    assert(it->is_server == server);

    return true;

}

bool DI_database::RemoveParticipant(const GUID_t& deceased)
{
    std::lock_guard<std::mutex> lock(database_mutex);

    return image.erase(deceased) != 0;
}

bool DI_database::RemoveParticipant(
    const GUID_t& spokesman,
    const GUID_t& ptid)
{
    std::lock_guard<std::mutex> lock(database_mutex);

    PtDB & _database = image[spokesman];
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
bool DI_database::AddEndPoint(
    T&(PtDI::* m)() const,
    const GUID_t& spokesman,
    const GUID_t& ptid,
    const GUID_t& id,
    const std::string& _typename,
    const std::string& topicname)
{
    std::lock_guard<std::mutex> lock(database_mutex);

    PtDB & _database = image[spokesman];
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

    assert(_typename == sit->type_name);
    assert(topicname == sit->topic_name);

    return true;
}

template<class T>
bool DI_database::RemoveEndPoint(
    T&(PtDI::* m)() const,
    const GUID_t& spokesman,
    const GUID_t& ptid,
    const GUID_t& id)
{
    std::lock_guard<std::mutex> lock(database_mutex);

    PtDB & _database = image[spokesman];
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

    if (it->CountEndpoints() == 0 && !it->is_alive)
    {
        // remove participant if zombie
        _database.erase(it);
    }

    return true;

}

bool DI_database::AddSubscriber(
    const GUID_t& spokesman,
    const GUID_t& ptid,
    const GUID_t& sid,
    const std::string& _typename,
    const std::string& topicname)
{
    return AddEndPoint(&PtDI::getSubscribers, spokesman,ptid, sid, _typename, topicname);
}

bool DI_database::RemoveSubscriber(
    const GUID_t& spokesman,
    const GUID_t& ptid,
    const GUID_t& sid)
{
    return RemoveEndPoint(&PtDI::getSubscribers, spokesman, ptid, sid);
}

bool DI_database::AddPublisher(
    const GUID_t& spokesman,
    const GUID_t& ptid,
    const GUID_t& pid,
    const std::string& _typename,
    const std::string& topicname)
{
    return AddEndPoint(&PtDI::getPublishers, spokesman, ptid, pid, _typename, topicname);
}

bool DI_database::RemovePublisher(
    const GUID_t& spokesman,
    const GUID_t& ptid,
    const GUID_t& pid)
{
    return RemoveEndPoint(&PtDI::getPublishers, spokesman, ptid, pid);
}

DI_database::size_type DI_database::CountParticipants(const GUID_t& spokesman) const
{
    std::lock_guard<std::mutex> lock(database_mutex);

    const PtDB* p = image[spokesman];

    if (p != nullptr)
    {
       return p->size();
    }

    return 0;
}

DI_database::size_type DI_database::CountSubscribers(const GUID_t& spokesman) const
{
    std::lock_guard<std::mutex> lock(database_mutex);

    const PtDB* p = image[spokesman];

    if (p != nullptr)
    {
        size_type count = 0;

        for (const PtDI& part : *p)
        {
            count += part.subscribers.size();
        }

        return count;
    }

    return 0;
}

DI_database::size_type DI_database::CountPublishers(const GUID_t& spokesman) const
{
    std::lock_guard<std::mutex> lock(database_mutex);

    const PtDB* p = image[spokesman];

    if (p != nullptr)
    {
        size_type count = 0;

        for (const PtDI& part : *p)
        {
            count += part.publishers.size();
        }

        return count;
    }

    return 0;
}

DI_database::size_type DI_database::CountSubscribers(
    const GUID_t& spokesman,
    const GUID_t& ptid) const
{
    std::lock_guard<std::mutex> lock(database_mutex);

    const PtDB* p = image[spokesman];

    if (p != nullptr)
    {
        const PtDB & _database = *p;
        PtDB::iterator it = std::lower_bound(_database.begin(), _database.end(), ptid);

        if (it == _database.end() || *it != ptid)
        {
            // participant is no there
            return 0;
        }

        return it->subscribers.size();
    }

    return 0;
}

DI_database::size_type DI_database::CountPublishers(
    const GUID_t& spokesman,
    const GUID_t& ptid) const
{
    std::lock_guard<std::mutex> lock(database_mutex);

    const PtDB* p = image[spokesman];

    if (p != nullptr)
    {
        const PtDB & _database = *p;
        PtDB::iterator it = std::lower_bound(_database.begin(), _database.end(), ptid);

        if (it == _database.end() || *it != ptid)
        {
            // participant is no there
            return 0;
        }

        return it->publishers.size();
    }

    return 0;
}

bool eprosima::discovery_server::operator==(const PtDB& l,const PtDB& r)
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
                || ((rit->endpoint_guid == l.endpoint_guid || rit->endpoint_guid == r.endpoint_guid) && r.end() == ++rit));
        }

        if (rit == r.end())
        {
            // finish simultaneously or differ only in
            // each other discovery data
            return (lit == l.end()
                || ((lit->endpoint_guid == r.endpoint_guid || lit->endpoint_guid == l.endpoint_guid) && l.end() == ++lit));
        }

        // comparing elements
        if (lit->endpoint_guid == rit->endpoint_guid)
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

            if (lit++->endpoint_guid == r.endpoint_guid)
            {
                go = true; // sweep over
            }

            if (rit++->endpoint_guid == l.endpoint_guid)
            {
                go = true; // sweep over
            }
        }

    };

    // one or several unknown participants within
    return false;
}

PtDB& Snapshot::operator[](const GUID_t& id)
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

const PtDB* Snapshot::operator[](const GUID_t& id) const
{
    auto it = std::lower_bound(begin(), end(), id);

    if (it == end() || *it != id)
    {
        return nullptr; // no there
    }

    return &*it;
}

void Snapshot::to_xml(
    tinyxml2::XMLElement* pRoot,
    tinyxml2::XMLDocument& xmlDoc) const
{
    using namespace tinyxml2;

    //XMLElement* pTimestamp = xmlDoc.NewElement(s_sTimestamp.c_str());
    //pTimestamp->SetText(this->_time.time_since_epoch().count());
    //pRoot->InsertEndChild(pTimestamp);
    pRoot->SetAttribute(s_sTimestamp.c_str(), this->_time.time_since_epoch().count());
    pRoot->SetAttribute(s_sSomeone.c_str(), if_someone);

    XMLElement* pDescription = xmlDoc.NewElement(s_sDescription.c_str());
    pDescription->SetText(this->_des.c_str());
    pRoot->InsertEndChild(pDescription);

    for (const PtDB& ptdb : *this)
    {
        XMLElement* pPtdb = xmlDoc.NewElement(s_sPtDB.c_str());
        {
            std::stringstream sstream;
            sstream << ptdb.endpoint_guid.guidPrefix;
            pPtdb->SetAttribute(s_sGUID_prefix.c_str(), sstream.str().c_str());
        }
        {
            std::stringstream sstream;
            sstream << ptdb.endpoint_guid.entityId;
            pPtdb->SetAttribute(s_sGUID_entity.c_str(), sstream.str().c_str());
        }

        for (const PtDI& ptdi : ptdb)
        {
            XMLElement* pPtdi = xmlDoc.NewElement(s_sPtDI.c_str());
            {
                std::stringstream sstream;
                sstream << ptdi.endpoint_guid.guidPrefix;
                pPtdi->SetAttribute(s_sGUID_prefix.c_str(),sstream.str().c_str());
            }
            {
                std::stringstream sstream;
                sstream << ptdi.endpoint_guid.entityId;
                pPtdi->SetAttribute(s_sGUID_entity.c_str(),sstream.str().c_str());
            }

            pPtdi->SetAttribute(s_sServer.c_str(), ptdi.is_server);
            pPtdi->SetAttribute(s_sAlive.c_str(), ptdi.is_alive);
            pPtdi->SetAttribute(s_sName.c_str(), ptdi.participant_name.c_str());

            for (const SDI& sub : ptdi.subscribers)
            {
                XMLElement* pSub = xmlDoc.NewElement(s_sSubscriber.c_str());
                pSub->SetAttribute(s_sType.c_str(), sub.type_name.c_str());
                pSub->SetAttribute(s_sTopic.c_str(), sub.topic_name.c_str());
                {
                    std::stringstream sstream;
                    sstream << sub.endpoint_guid.guidPrefix;
                    pSub->SetAttribute(s_sGUID_prefix.c_str(),sstream.str().c_str());
                }
                {
                    std::stringstream sstream;
                    sstream << sub.endpoint_guid.entityId;
                    pSub->SetAttribute(s_sGUID_entity.c_str(),sstream.str().c_str());
                }
                pPtdi->InsertEndChild(pSub);
            }

            for (const PDI& pub : ptdi.publishers)
            {
                XMLElement* pPub = xmlDoc.NewElement(s_sPublisher.c_str());
                pPub->SetAttribute(s_sType.c_str(), pub.type_name.c_str());
                pPub->SetAttribute(s_sTopic.c_str(), pub.topic_name.c_str());
                {
                    std::stringstream sstream;
                    sstream << pub.endpoint_guid.guidPrefix;
                    pPub->SetAttribute(s_sGUID_prefix.c_str(),sstream.str().c_str());
                }
                {
                    std::stringstream sstream;
                    sstream << pub.endpoint_guid.entityId;
                    pPub->SetAttribute(s_sGUID_entity.c_str(),sstream.str().c_str());
                }
                pPtdi->InsertEndChild(pPub);
            }

            pPtdb->InsertEndChild(pPtdi);
        }

        pRoot->InsertEndChild(pPtdb);
    }
}

void Snapshot::from_xml(tinyxml2::XMLElement* pRoot)
{
    using namespace tinyxml2;
    using eprosima::fastrtps::rtps::GuidPrefix_t;
    using eprosima::fastrtps::rtps::EntityId_t;

    if (pRoot != nullptr)
    {
        std::string timestamp = pRoot->Attribute(s_sTimestamp.c_str());
        if (!timestamp.empty())
        {
            uint64_t time = std::stoull(timestamp);
            //pTimestamp->QueryInt64Text(&time);
            using Ns = std::chrono::nanoseconds;

            _time = std::chrono::steady_clock::time_point(Ns(time));
            //std::cout << "Timestamp: " << timestamp << std::endl;
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
            PtDB ptdb(ptdb_guid);

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
                PtDI ptdi(ptdi_guid);

                pPtdi->QueryBoolAttribute(s_sServer.c_str(), &ptdi.is_server);
                pPtdi->QueryBoolAttribute(s_sAlive.c_str(), &ptdi.is_alive);
                ptdi.participant_name = pPtdi->Attribute(s_sName.c_str());
                //std::cout << "PTDI: " << ptdi.id_ << std::endl;

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
                    SDI sub(sub_guid, pSub->Attribute(s_sType.c_str()), pSub->Attribute(s_sTopic.c_str()));
                    ptdi.subscribers.insert(std::move(sub));
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
                    PDI pub(pub_guid, pPub->Attribute(s_sType.c_str()), pPub->Attribute(s_sTopic.c_str()));
                    ptdi.publishers.insert(std::move(pub));
                }

                ptdb.insert(std::move(ptdi));
            }

            this->insert(std::move(ptdb));

        }
    }/*
    else
    {
        LOG_ERROR("Error loading XML file " << file);
    }*/
}

Snapshot& Snapshot::operator+=(const Snapshot& sh)
{
    this->insert(sh.begin(), sh.end());
    return *this;
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
