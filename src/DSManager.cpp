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


#include <tinyxml2.h>

#include <fastrtps/Domain.h>
#include <fastrtps/xmlparser/XMLProfileManager.h>

#include <fastrtps/subscriber/Subscriber.h>
#include <fastrtps/publisher/Publisher.h>

#include "DSManager.h"

#include <iostream>
#include <sstream>


using namespace eprosima::fastrtps;
using namespace eprosima::discovery_server;

// String literals:
// brand new
static const std::string s_sDS("DS");
static const std::string s_sServers("servers");
static const std::string s_sServer("server");
static const std::string s_sClients("clients");
static const std::string s_sClient("client");
static const std::string s_sPersist("persist");
static const std::string s_sLP("ListeningPorts");
static const std::string s_sSL("ServersList");
static const std::string s_sRServer("RServer");
static const std::string s_sTime("time");
static const std::string s_sCreationTime("creation_time");
static const std::string s_sRemovalTime("removal_time");
static const std::string s_sSnapshot("snapshot");
static const std::string s_sSnapshots("snapshots");
static const std::string s_sUserShutdown("user_shutdown");

// non exported from fast-RTPS (watch out they are updated)
namespace eprosima{
    namespace fastrtps{
        namespace xmlparser
        {
            const char* PROFILES = "profiles";
            const char* PROFILE_NAME = "profile_name";
            const char* PREFIX = "prefix";
            const char* NAME = "name";
            const char* META_UNI_LOC_LIST = "metatrafficUnicastLocatorList";
            const char* META_MULTI_LOC_LIST = "metatrafficMulticastLocatorList";
            const char* TYPES = "types";
            const char* PUBLISHER = "publisher";
            const char* SUBSCRIBER = "subscriber";
            const char* TOPIC = "topic";
        }
    }
}

/*static members*/
HelloWorldPubSubType DSManager::_defaultType;
TopicAttributes DSManager::_defaultTopic("HelloWorldTopic", "HelloWorld");

DSManager::DSManager(const std::string &xml_file_path)
    : _active(false), _nocallbacks(false), _shutdown(false)
{
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(xml_file_path.c_str()) == tinyxml2::XMLError::XML_SUCCESS)
    {
        tinyxml2::XMLElement *root = doc.FirstChildElement(s_sDS.c_str());
        if (!root)
        {
            LOG("Invalid config file");
            return;
        }

        // try load the user_shutdown attribute
        _shutdown = !root->BoolAttribute(s_sUserShutdown.c_str(),!_shutdown);

        for (auto child = doc.FirstChildElement(s_sDS.c_str());
            child != nullptr; child = child->NextSiblingElement(s_sDS.c_str()))
        {
            // first make XMLProfileManager::loadProfiles parse the config file. Afterwards loaded info is accessible
            // through XMLProfileManager::fillParticipantAttributes and related

            tinyxml2::XMLElement *profiles = child->FirstChildElement(xmlparser::PROFILES);
            if (profiles)
            {
                loadProfiles(profiles);
            }
            else
            {
                LOG_ERROR("No profiles found!");
                return;
            }

            // Types parsing
            tinyxml2::XMLElement* types = child->FirstChildElement(xmlparser::TYPES);
            if (types)
            {
                if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::loadXMLDynamicTypes(*types))
                {
                    LOG_INFO("No dynamic type information loaded.");
                }
            }

            // Server processing requires a two pass analysis
            tinyxml2::XMLElement *servers = child->FirstChildElement(s_sServers.c_str());

            if (servers)
            {
            // 1. First map each server with his locators
                tinyxml2::XMLElement *server = servers->FirstChildElement(s_sServer.c_str());
                while (server)
                {
                    MapServerInfo(server);
                    server = server->NextSiblingElement(s_sServer.c_str());
                }

            // 2. Create the servers according with their configuration

                server = servers->FirstChildElement(s_sServer.c_str());
                while (server)
                {
                    loadServer(server);
                    server = server->NextSiblingElement(s_sServer.c_str());
                }
            }
            else
            {
                LOG_ERROR("No servers found!");
                return;
            }

            // Create the clients according with the configuration, clients have only testing purposes
            tinyxml2::XMLElement* clients = child->FirstChildElement(s_sClients.c_str());
            if (clients)
            {
                tinyxml2::XMLElement *client = clients->FirstChildElement(s_sClient.c_str());
                while (client)
                {
                    loadClient(client);
                    client = client->NextSiblingElement(s_sClient.c_str());
                }
            }

            // Create snapshot events
            tinyxml2::XMLElement* snapshots = child->FirstChildElement(s_sSnapshots.c_str());
            if (snapshots)
            {
                tinyxml2::XMLElement *snapshot = snapshots->FirstChildElement(s_sSnapshot.c_str());
                while (snapshot)
                {
                    loadSnapshot(snapshot);
                    snapshot = snapshot->NextSiblingElement(s_sSnapshot.c_str());
                }
            }

        }

        // at least one server must be created from config file
        if (_servers.size() > 0)
        {
            _active = true;
        }
    }
    else
    {
        LOG("Config file not found.");
    }

    LOG_INFO("File " << xml_file_path << " parsed successfully.");
}

void DSManager::runEvents(std::istream& in /*= std::cin*/, std::ostream& out /*= std::cout*/)
{
    // Order the event list
    std::sort(_events.begin(), _events.end(), [](LJD* p1, LJD* p2) -> bool { return *p1 < *p2; });

    // traverse the list
    for (LJD* p : _events)
    {
        // Wait till specified time
        p->Wait();
        // execute
        (*p)(*this);
    }

    // Wait for user shutdown
    if (!_shutdown)
    {
        out << "\n### Discovery Server is running, press any key to quit ###" << std::endl;
        out.flush();
        in.ignore();
    }
}

void DSManager::addServer(Participant* s)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);
    assert(_servers[s->getGuid()] == nullptr);

    _servers[s->getGuid()] = s;
    _active = true;
}

void DSManager::addClient(Participant* c)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);
    assert(_clients[c->getGuid()] == nullptr);
    _clients[c->getGuid()] = c;
}

Participant * DSManager::getParticipant(GUID_t & id)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);

    // first in clients
    participant_map::iterator it = _clients.find(id);
    if (it != _clients.end())
    {
        return it->second;
    }
    else if ((it = _servers.find(id)) != _servers.end())
    {
        return it->second;
    }

    return nullptr;
}

Participant * DSManager::removeParticipant(GUID_t & id)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);

    Participant * ret = nullptr;

    // remove any related pubs-subs
    {
        publisher_map paux;
        std::remove_copy_if(_pubs.begin(), _pubs.end(), std::inserter(paux, paux.begin()),
            [&id](publisher_map::value_type it) { return id.guidPrefix == it.first.guidPrefix; });
        _pubs.swap(paux);

        subscriber_map saux;
        std::remove_copy_if(_subs.begin(), _subs.end(), std::inserter(saux, saux.begin()),
            [&id](subscriber_map::value_type it) { return id.guidPrefix == it.first.guidPrefix; });
        _subs.swap(saux);
    }

    // first in clients
    participant_map::iterator it = _clients.find(id);
    if (it != _clients.end())
    {
        ret = it->second;
        _clients.erase(it);
    }
    else if ( ( it = _servers.find(id) ) != _servers.end())
    {
        ret = it->second;
        _servers.erase(it);
    }

    return ret;
}

void DSManager::addSubscriber(Subscriber * sub)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);
    assert(_subs[sub->getGuid()] == nullptr);
    _subs[sub->getGuid()] = sub;
}

Subscriber * DSManager::getSubscriber(GUID_t & id)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);

    subscriber_map::iterator it = _subs.find(id);
    if (it != _subs.end())
    {
        return it->second;
    }

    return nullptr;
}

Subscriber * DSManager::removeSubscriber(GUID_t & id)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);

    Subscriber * ret = nullptr;

    subscriber_map::iterator it = _subs.find(id);
    if (it != _subs.end())
    {
        ret = it->second;
        _subs.erase(it);
    }

    return ret;
}

void DSManager::addPublisher(Publisher * pub)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);
    assert(_pubs[pub->getGuid()] == nullptr);
    _pubs[pub->getGuid()] = pub;
}

Publisher * DSManager::getPublisher(GUID_t & id)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);

    publisher_map::iterator it = _pubs.find(id);
    if (it != _pubs.end())
    {
        return it->second;
    }

    return nullptr;
}

Publisher * DSManager::removePublisher(GUID_t & id)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);

    Publisher * ret = nullptr;

    publisher_map::iterator it = _pubs.find(id);
    if (it != _pubs.end())
    {
        ret = it->second;
        _pubs.erase(it);
    }

    return ret;
}

types::DynamicPubSubType * DSManager::getType(std::string & name)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);

    type_map::iterator it = _types.find(name);
    if (it != _types.end())
    {
        return it->second;
    }

    return nullptr;
}

types::DynamicPubSubType * DSManager::setType(std::string & type_name)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);

    // Create dynamic type
    types::DynamicPubSubType* & pDt = _types[type_name];

    if (!pDt)
    {
        pDt = xmlparser::XMLProfileManager::CreateDynamicPubSubType(type_name);
        if (!pDt)
        {
            _types.erase(type_name);

            LOG_ERROR("DSManager::setType couldn't create type " << type_name);
            return nullptr;
        }
    }

    return pDt;
}

void DSManager::loadProfiles(tinyxml2::XMLElement *profiles)
{
    xmlparser::XMLP_ret ret = xmlparser::XMLProfileManager::loadXMLProfiles(*profiles);

    if (ret == xmlparser::XMLP_ret::XML_OK)
    {
        LOG_INFO("Profiles parsed successfully.");
    }
    else
    {
        LOG_ERROR("Error parsing profiles!");
    }
}


void DSManager::onTerminate()
{
    {   // make sure all other threads don't modify the state
        std::lock_guard<std::recursive_mutex> lock(_mtx);

        if (_nocallbacks)
        {
            return; // DSManager already terminated
        }

        _nocallbacks = true;
    }

    // release all subscribers
    for (const auto&s : _subs)
    {
        Domain::removeSubscriber(s.second);
    }

    _subs.clear();

    // release all publishers
    for (const auto&p : _pubs)
    {
        Domain::removePublisher(p.second);
    }

    _pubs.clear();

    // the servers are appended because they should be destroyed at the end
    _clients.insert(_servers.begin(), _servers.end());

    for (const auto &e : _clients)
    {
        Participant *p = e.second;
        if (p)
        {
            Domain::removeParticipant(p);
        }
    }

    _servers.clear();
    _clients.clear();

    //unregister the types
    for (const auto & t : _types)
    {
        xmlparser::XMLProfileManager::DeleteDynamicPubSubType(t.second);
    }

    _types.clear();

    // remove all events
    for (auto ptr : _events)
    {
        delete ptr;
    }

    _events.clear();
}

bool DSManager::isActive()
{
    return _active;
}


DSManager::~DSManager()
{
    onTerminate();
}

void DSManager::loadServer(tinyxml2::XMLElement* server)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);

    // check if we need to create an event
    std::chrono::steady_clock::time_point creation_time, removal_time;
    creation_time = removal_time = getTime();

    {
        const char * creation_time_str = server->Attribute(s_sCreationTime.c_str());
        if (creation_time_str)
        {
            int aux;
            std::istringstream(creation_time_str) >> aux;
            creation_time += std::chrono::seconds(aux);
        }

        const char * removal_time_str = server->Attribute(s_sRemovalTime.c_str());
        if (removal_time_str)
        {
            int aux;
            std::istringstream(removal_time_str) >> aux;
            removal_time += std::chrono::seconds(aux);
        }
    }

    // profile name is mandatory
    const char * profile_name = server->Attribute(xmlparser::PROFILE_NAME);

    if (!profile_name)
    {
        LOG_ERROR(xmlparser::PROFILE_NAME << " is a mandatory attribute of server tag");
        return;
    }

    // retrieve profile attributes
    ParticipantAttributes atts;
    if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::fillParticipantAttributes(std::string(profile_name), atts))
    {
        LOG_ERROR("DSManager::loadServer couldn't load profile " << profile_name);
        return;
    }

    // server name is either pass as an attribute (preferred to allow profile reuse) or inside the profile
    const char * name = server->Attribute(xmlparser::NAME);
    if (name)
    {
        atts.rtps.setName(name);
    }

    // server GuidPrefix is either pass as an attribute (preferred to allow profile reuse)
    // or inside the profile.
    GuidPrefix_t & prefix = atts.rtps.prefix;
    const char * cprefix = server->Attribute(xmlparser::PREFIX);

    if (cprefix && !(std::istringstream(cprefix) >> prefix)
        && (prefix == c_GuidPrefix_Unknown))
    {
        LOG_ERROR("Servers cannot have a framework provided prefix"); // at least for now
        return;
    }

    GUID_t guid(prefix, c_EntityId_RTPSParticipant);

    // Check if the guidPrefix is already in use (there is a mistake on config file)
    if (_servers.find(guid) != _servers.end())
    {
        LOG_ERROR("DSManager detected two servers sharing the same prefix " << prefix);
        return;
    }

    // replace the atts.rtps.builtin lists with the ones from _server_locators (if present)
    // note that a previous call to DSManager::MapServerInfo
    serverLocator_map::mapped_type & lists = _server_locators[guid];
    if (!lists.first.empty() || !lists.second.empty())
    {
        // server elements take precedence over profile ones
        // I copy them because other servers may need this values
        atts.rtps.builtin.metatrafficMulticastLocatorList = lists.first;
        atts.rtps.builtin.metatrafficUnicastLocatorList = lists.second;
    }

    // load the server list (if present) and update the atts.rtps.builtin
    tinyxml2::XMLElement *server_list = server->FirstChildElement(s_sSL.c_str());

    if (server_list)
    {
        RemoteServerList_t & list = atts.rtps.builtin.m_DiscoveryServers;
        list.clear(); // server elements take precedence over profile ones

        tinyxml2::XMLElement * rserver = server_list->FirstChildElement(s_sRServer.c_str());

        while (rserver)
        {
            RemoteServerList_t::value_type srv;
            GuidPrefix_t & prefix = srv.guidPrefix;

            // load the prefix
            const char * cprefix = rserver->Attribute(xmlparser::PREFIX);

            if (cprefix && !(std::istringstream(cprefix) >> prefix)
                && (prefix == c_GuidPrefix_Unknown))
            {
                LOG_ERROR("RServers must provide a prefix"); // at least for now
                return;
            }

            // load the locator lists
            serverLocator_map::mapped_type & lists = _server_locators[srv.GetParticipant()];
            srv.metatrafficMulticastLocatorList = lists.first;
            srv.metatrafficUnicastLocatorList = lists.second;

            list.push_back(std::move(srv));

            rserver = rserver->NextSiblingElement(s_sRServer.c_str());
        }
    }

    // We define the PDP as external (when moved to fast library it would be SERVER)
    BuiltinAttributes & b = atts.rtps.builtin;
    assert(b.discoveryProtocol == SERVER || b.discoveryProtocol == BACKUP);

    // Create the participant or the associated events
    DPC event(creation_time, std::move(atts), &DSManager::addServer);

    // at least one server will be present
    _active = true;

    if (creation_time == getTime())
    {
        event(*this);
    }
    else
    {   // late joiner
        _events.push_back(new DPC(std::move(event)));
    }

    if (removal_time != getTime())
    {
        // early leaver
        _events.push_back(new DPD(removal_time, guid));
    }

    // Once the participant is created we create the associated endpoints
    tinyxml2::XMLElement* pub = server->FirstChildElement(xmlparser::PUBLISHER);
    while (pub)
    {
        loadPublisher(guid,pub);
        pub = pub->NextSiblingElement(xmlparser::PUBLISHER);
    }

    tinyxml2::XMLElement* sub = server->FirstChildElement(xmlparser::SUBSCRIBER);
    while (sub)
    {
        loadSubscriber(guid,sub);
        sub = sub->NextSiblingElement(xmlparser::SUBSCRIBER);
    }
}


void DSManager::loadClient(tinyxml2::XMLElement* client)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);

    // check if we need to create an event
    std::chrono::steady_clock::time_point creation_time, removal_time;
    creation_time = removal_time = getTime();

    {
        const char * creation_time_str = client->Attribute(s_sCreationTime.c_str());
        if (creation_time_str)
        {
            int aux;
            std::istringstream(creation_time_str) >> aux;
            creation_time += std::chrono::seconds(aux);
        }

        const char * removal_time_str = client->Attribute(s_sRemovalTime.c_str());
        if (removal_time_str)
        {
            int aux;
            std::istringstream(removal_time_str) >> aux;
            removal_time += std::chrono::seconds(aux);
        }
    }

    // clients are created for debugging purposes
    // profile name is mandatory because they must reference servers
    const char * profile_name = client->Attribute(xmlparser::PROFILE_NAME);

    if (!profile_name)
    {
        LOG_ERROR(xmlparser::PROFILE_NAME << " is a mandatory attribute of client tag");
        return;
    }

    // retrieve profile attributes
    ParticipantAttributes atts;
    if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::fillParticipantAttributes(std::string(profile_name), atts))
    {
        LOG_ERROR("DSManager::loadClient couldn't load profile " << profile_name);
        return;
    }

    // we must assert that PDPtype is CLIENT
    if (atts.rtps.builtin.discoveryProtocol != rtps::PDPType_t::CLIENT)
    {
        LOG_ERROR("DSManager::loadClient try to create a client with an incompatible profile: " << profile_name);
        return;
    }

    // pick the client's name (isn't mandatory to provide it). Takes precedence over profile provided.
    const char * name = client->Attribute(xmlparser::NAME);
    if( name != nullptr )
    {
        atts.rtps.setName(name);
    }

    // server may be provided by prefix (takes precedence) or by list
    const char * server = client->Attribute(s_sServer.c_str());
    if (server )
    {
        RemoteServerList_t::value_type srv;
        GuidPrefix_t & prefix = srv.guidPrefix;

        if (!(std::istringstream(server) >> prefix)
            && (prefix == c_GuidPrefix_Unknown))
        {
            LOG_ERROR("server attribute must provide a prefix"); // at least for now
            return;
        }

        RemoteServerList_t & list = atts.rtps.builtin.m_DiscoveryServers;
        list.clear(); // server elements take precedence over profile ones

        // load the locator lists
        serverLocator_map::mapped_type & lists = _server_locators[srv.GetParticipant()];
        srv.metatrafficMulticastLocatorList = lists.first;
        srv.metatrafficUnicastLocatorList = lists.second;

        list.push_back(std::move(srv));

    }
    else
    {
        // load the server list (if present) and update the atts.rtps.builtin
        tinyxml2::XMLElement *server_list = client->FirstChildElement(s_sSL.c_str());

        if (server_list)
        {
            RemoteServerList_t & list = atts.rtps.builtin.m_DiscoveryServers;
            list.clear(); // server elements take precedence over profile ones

            tinyxml2::XMLElement * rserver = server_list->FirstChildElement(s_sRServer.c_str());

            while (rserver)
            {
                RemoteServerList_t::value_type srv;
                GuidPrefix_t & prefix = srv.guidPrefix;

                // load the prefix
                const char * cprefix = rserver->Attribute(xmlparser::PREFIX);

                if (cprefix && !(std::istringstream(cprefix) >> prefix)
                    && (prefix == c_GuidPrefix_Unknown))
                {
                    LOG_ERROR("RServers must provide a prefix"); // at least for now
                    return;
                }

                // load the locator lists
                serverLocator_map::mapped_type & lists = _server_locators[srv.GetParticipant()];
                srv.metatrafficMulticastLocatorList = lists.first;
                srv.metatrafficUnicastLocatorList = lists.second;

                list.push_back(std::move(srv));

                rserver = rserver->NextSiblingElement(s_sRServer.c_str());
            }
        }

    }

    GUID_t guid(atts.rtps.prefix, c_EntityId_RTPSParticipant);
    DPD * pD = nullptr;
    DPC * pC = nullptr;

    if (removal_time != getTime())
    {
        // early leaver
        pD = new DPD(removal_time, guid);
        _events.push_back(pD);
    }

    // Create the participant or the associated events
    DPC event(creation_time, std::move(atts), &DSManager::addClient,pD);

    if (creation_time == getTime())
    {
        event(*this);

        // get the client guid
        guid = event._guid;
    }
    else
    {   // late joiner
        pC = new DPC(std::move(event));
        _events.push_back(pC);
    }

    // Once the participant is created we create the associated endpoints
    tinyxml2::XMLElement* pub = client->FirstChildElement(xmlparser::PUBLISHER);
    while (pub)
    {
        loadPublisher(guid,pub,pC);
        pub = pub->NextSiblingElement(xmlparser::PUBLISHER);
    }

    tinyxml2::XMLElement* sub = client->FirstChildElement(xmlparser::SUBSCRIBER);
    while (sub)
    {
        loadSubscriber(guid,sub,pC);
        sub = sub->NextSiblingElement(xmlparser::SUBSCRIBER);
    }

}

void DSManager::loadSubscriber(GUID_t & part_guid, tinyxml2::XMLElement* sub, DPC* pLJ /*= nullptrt*/)
{
    assert(sub != nullptr);

    std::lock_guard<std::recursive_mutex> lock(_mtx);

    // retrieve participant
    Participant * part = getParticipant(part_guid);

    // check if we need to create an event
    std::chrono::steady_clock::time_point creation_time, removal_time;
    creation_time = removal_time = getTime();

    {
        const char * creation_time_str = sub->Attribute(s_sCreationTime.c_str());
        if (creation_time_str)
        {
            int aux;
            std::istringstream(creation_time_str) >> aux;
            creation_time += std::chrono::seconds(aux);
        }
        else if (!part)
        {
            // late joiners must spawn late joiners
            LOG_ERROR("DSManager::loadSubscriber tries to create a subscriber to a late joiner.");
            return;
        }

        const char * removal_time_str = sub->Attribute(s_sRemovalTime.c_str());
        if (removal_time_str)
        {
            int aux;
            std::istringstream(removal_time_str) >> aux;
            removal_time += std::chrono::seconds(aux);
        }
    }


    // subscribers are created for debugging purposes
    // default topic is the static HelloWorld one
    const char * profile_name = sub->Attribute(xmlparser::PROFILE_NAME);

    SubscriberAttributes * subatts = new SubscriberAttributes();

    if (!profile_name)
    {
        // get default subscriber attributes
        xmlparser::XMLProfileManager::getDefaultSubscriberAttributes(*subatts);
    }
    else
    {
        // try load from profile
        if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::fillSubscriberAttributes(std::string(profile_name), *subatts))
        {
            LOG_ERROR("DSManager::loadSubscriber couldn't load profile " << profile_name);
            return;
        }
    }

    // see if topic is specified
    const char * topic_name = sub->Attribute(xmlparser::TOPIC);

    if (topic_name)
    {
        if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::fillTopicAttributes(std::string(topic_name), subatts->topic))
        {
            LOG_ERROR("DSManager::loadSubscriber couldn't load topic profile " << profile_name);
            return;
        }
    }

    DED<Subscriber> * pDE = nullptr; // subscriber destruction event

    if (removal_time != getTime())
    {
        // Destruction event needs the endpoint guid 
        // that would be provided in creation
        pDE = new DED<Subscriber>(removal_time);
        _events.push_back(pDE);
    }

    DEC<Subscriber> event(creation_time, subatts, part_guid, pDE, pLJ);

    if (creation_time == getTime())
    {
        event(*this);
    }
    else
    {   // late joiner
        _events.push_back(new DEC<Subscriber>(std::move(event)));
    }

}

void DSManager::loadPublisher(GUID_t & part_guid, tinyxml2::XMLElement* sub, DPC* pLJ /*= nullptrt*/)
{
    assert( sub != nullptr);

    std::lock_guard<std::recursive_mutex> lock(_mtx);

    // retrieve participant
    Participant * part = getParticipant(part_guid);

    // check if we need to create an event
    std::chrono::steady_clock::time_point creation_time, removal_time;
    creation_time = removal_time = getTime();

    {
        const char * creation_time_str = sub->Attribute(s_sCreationTime.c_str());
        if (creation_time_str)
        {
            int aux;
            std::istringstream(creation_time_str) >> aux;
            creation_time += std::chrono::seconds(aux);
        }
        else if (!part)
        {
            // late joiners must spawn late joiners
            LOG_ERROR("DSManager::loadSubscriber tries to create a subscriber to a late joiner.");
            return;
        }

        const char * removal_time_str = sub->Attribute(s_sRemovalTime.c_str());
        if (removal_time_str)
        {
            int aux;
            std::istringstream(removal_time_str) >> aux;
            removal_time += std::chrono::seconds(aux);
        }
    }

    // subscribers are created for debugging purposes
    // default topic is the static HelloWorld one
    const char * profile_name = sub->Attribute(xmlparser::PROFILE_NAME);

    PublisherAttributes * pubatts = new PublisherAttributes();

    if (!profile_name)
    {
        // get default subscriber attributes
        xmlparser::XMLProfileManager::getDefaultPublisherAttributes(*pubatts);
    }
    else
    {
        // try load from profile
        if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::fillPublisherAttributes(std::string(profile_name), *pubatts))
        {
            LOG_ERROR("DSManager::loadPublisher couldn't load profile " << profile_name);
            return;
        }
    }

    // see if topic is specified
    const char * topic_name = sub->Attribute(xmlparser::TOPIC);

    if (topic_name)
    {
        if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::fillTopicAttributes(std::string(topic_name), pubatts->topic))
        {
            LOG_ERROR("DSManager::loadPublisher couldn't load topic profile ");
            return;
        }
    }

    DED<Publisher> * pDE = nullptr; // subscriber destruction event

    if (removal_time != getTime())
    {
        // Destruction event needs the endpoint guid 
        // that would be provided in creation
        pDE = new DED<Publisher>(removal_time);
        _events.push_back(pDE);
    }

    DEC<Publisher> event(creation_time, pubatts, part_guid, pDE, pLJ);

    if (creation_time == getTime())
    {
        event(*this);
    }
    else
    {   // late joiner
       _events.push_back(new DEC<Publisher>(std::move(event)));
    }
}

std::chrono::steady_clock::time_point DSManager::getTime() const
{
    return _state.getTime();
}

void DSManager::loadSnapshot(tinyxml2::XMLElement* snapshot)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);

    // snapshots are created for debugging purposes
    // time is mandatory 
    const char * time_str = snapshot->Attribute(s_sTime.c_str());
    
    if (!time_str)
    {
        LOG_ERROR(s_sTime << " is a mandatory attribute of " << s_sSnapshot << " tag");
        return;
    }

    std::chrono::steady_clock::time_point time(getTime());
    {
        int aux;
        std::istringstream(time_str) >> aux;
        time += std::chrono::seconds(aux);
    }

    // Get the description from the tag
    std::string description(snapshot->GetText());

    // Add the event
    _events.push_back(new DS(time, description));
}


void DSManager::MapServerInfo(tinyxml2::XMLElement* server)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);

    uint8_t ident = 1;

    // profile name is mandatory
    std::string profile_name(server->Attribute(xmlparser::PROFILE_NAME));

    if (profile_name.empty())
    {
        LOG_ERROR(xmlparser::PROFILE_NAME << " is a mandatory attribute of server tag");
        return;
    }

    // server GuidPrefix is either pass as an attribute (preferred to allow profile reuse)
    // or inside the profile.
    GuidPrefix_t prefix;
    std::shared_ptr<ParticipantAttributes> atts;

    const char * cprefix = server->Attribute(xmlparser::PREFIX);

    if (cprefix)
    {
        std::istringstream(cprefix) >> prefix;
    }
    else
    {
        // I must load the prefix from the profile
        // retrieve profile attributes
        atts = std::make_shared<ParticipantAttributes>();
        if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::fillParticipantAttributes(profile_name, *atts))
        {
            LOG_ERROR("DSManager::loadServer couldn't load profile " << profile_name);
            return;
        }

        prefix = atts->rtps.prefix;
    }

    if (prefix == c_GuidPrefix_Unknown)
    {
        LOG_ERROR("Servers cannot have a framework provided prefix"); // at least for now
        return;
    }

    // Now we search the locator lists
    serverLocator_map::mapped_type pair;

    tinyxml2::XMLElement *LP = server->FirstChildElement(s_sLP.c_str());
    if (LP)
    {
        tinyxml2::XMLElement * list = LP->FirstChildElement(xmlparser::META_MULTI_LOC_LIST);

        if (list && (xmlparser::XMLP_ret::XML_OK != getXMLLocatorList(list, pair.first, ident)))
        {
            LOG_ERROR("Server " << prefix << " has an ill formed " << xmlparser::META_MULTI_LOC_LIST );
        }

        list = LP->FirstChildElement(xmlparser::META_UNI_LOC_LIST);
        if (list && (xmlparser::XMLP_ret::XML_OK != getXMLLocatorList(list, pair.second, ident)))
        {
            LOG_ERROR("Server " << prefix << " has an ill formed " << xmlparser::META_UNI_LOC_LIST);
        }

    }
    else
    {
        LocatorList_t multicast, unicast;

        // retrieve profile attributes
        if (!atts)
        {
            atts = std::make_shared<ParticipantAttributes>();
            if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::fillParticipantAttributes(profile_name, *atts))
            {
                LOG_ERROR("DSManager::loadServer couldn't load profile " << profile_name);
                return;
            }
        }

        pair.first = atts->rtps.builtin.metatrafficMulticastLocatorList;
        pair.second = atts->rtps.builtin.metatrafficUnicastLocatorList;
    }

    // now save the value
    _server_locators[GUID_t(prefix, c_EntityId_RTPSParticipant)] = std::move(pair);

}

void DSManager::onParticipantDiscovery(Participant* participant, rtps::ParticipantDiscoveryInfo&& info)
{
    bool server = false;
    GUID_t & partid = info.info.m_guid;

    LOG_INFO("Participant " << participant->getAttributes().rtps.getName() << " reports a participant "
        << info.info.m_participantName << " is " << info.status << ". Prefix " << partid);

    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);

        if (!_nocallbacks)
        {
            // DSManager info still valid
            server = _servers.end() != _servers.find(partid);
        }
    }

    // add to database, it has its own mtx
    // note that when a participant is destroyed he will wait for all his callbacks to return
    // _state will be alive during all callbacks
    switch (info.status)
    {
        case ParticipantDiscoveryInfo::DISCOVERY_STATUS::DISCOVERED_PARTICIPANT:
        {
            _state.AddParticipant(participant->getGuid(), partid, info.info.m_participantName, server);
            break;
        }
        case ParticipantDiscoveryInfo::DISCOVERY_STATUS::REMOVED_PARTICIPANT:
        {
            _state.RemoveParticipant(participant->getGuid(), partid);
            break;
        }
        case ParticipantDiscoveryInfo::CHANGED_QOS_PARTICIPANT:
        {
            break;
        }
        case ParticipantDiscoveryInfo::DROPPED_PARTICIPANT:
        {
            break;
        }
    }

    // note that I ignore DROPPED_PARTICIPANT because it deals with liveliness
    // and not with discovery messages
}

void DSManager::onSubscriberDiscovery(Participant* participant, rtps::ReaderDiscoveryInfo&& info)
{
    GUID_t & subsid = info.info.guid();
    GUID_t partid = iHandle2GUID(info.info.RTPSParticipantKey());

    // non reported info
    std::string part_name;

    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);

        participant_map::iterator it;

        if (!_nocallbacks)
        {
            // is one of ours?
            if ((it = _servers.find(partid)) != _servers.end() ||
                (it = _clients.find(partid)) != _clients.end())
            {
                part_name = it->second->getAttributes().rtps.getName();
            }
        }
        else
        {   // stick to non-DSManager info
            for(const PtDI* p : _state.FindParticipant(partid))
            {
                if (!p->_name.empty())
                {
                    part_name = p->_name;
                }
            }
        }

    }

    if (part_name.empty())
    {   // if remote use prefix instead of name
        part_name = (std::ostringstream() << partid).str();
    }

    _state.AddSubscriber(participant->getGuid(),partid, subsid, info.info.typeName(), info.info.topicName());

    LOG_INFO("Participant " << participant->getAttributes().rtps.getName() << " reports a subscriber of participant "
        << part_name << " is " << info.status << " with typename: " << info.info.typeName()
        << " topic: " << info.info.topicName()  << " GUID: " << subsid);
}

void  DSManager::onPublisherDiscovery(Participant* participant, rtps::WriterDiscoveryInfo&& info)
{
    GUID_t & pubsid = info.info.guid();
    GUID_t partid = iHandle2GUID(info.info.RTPSParticipantKey());

    // non reported info
    std::string part_name;

    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);

        if (!_nocallbacks)
        {
            // is one of ours?
            participant_map::iterator it;

            if ((it = _servers.find(partid)) != _servers.end() ||
                (it = _clients.find(partid)) != _clients.end())
            {
                part_name = it->second->getAttributes().rtps.getName();
            }
        }
        else
        {   // stick to non-DSManager info
            for (const PtDI* p : _state.FindParticipant(partid))
            {
                if (!p->_name.empty())
                {
                    part_name = p->_name;
                }
            }
        }
    }

    if (part_name.empty())
    {   // if remote use prefix instead of name
        part_name = (std::ostringstream() << partid).str();
    }

    _state.AddPublisher(participant->getGuid(), partid, pubsid, info.info.typeName(), info.info.topicName());

    LOG_INFO("Participant " << participant->getAttributes().rtps.getName() << " reports a publisher of participant "
        << part_name << " is " << info.status << " with typename: " << info.info.typeName()
        << " topic: " << info.info.topicName() << " GUID: " << pubsid);
}


std::ostream& eprosima::discovery_server::operator<<(std::ostream& o, ParticipantDiscoveryInfo::DISCOVERY_STATUS s)
{
    typedef ParticipantDiscoveryInfo::DISCOVERY_STATUS DS;

    switch (s)
    {
    case DS::DISCOVERED_PARTICIPANT:
        return o << "DISCOVERED_PARTICIPANT";
    case DS::CHANGED_QOS_PARTICIPANT:
        return o << "CHANGED_QOS_PARTICIPANT";
    case DS::REMOVED_PARTICIPANT:
        return o << "REMOVED_PARTICIPANT";
    case DS::DROPPED_PARTICIPANT:
        return o << "REMOVED_PARTICIPANT";
    default:    // unknown value, error
        o.setstate(std::ios::failbit);
    }

    return o;
}

std::ostream& eprosima::discovery_server::operator<<(std::ostream& o, ReaderDiscoveryInfo::DISCOVERY_STATUS s)
{
    typedef ReaderDiscoveryInfo::DISCOVERY_STATUS DS;

    switch (s)
    {
    case DS::DISCOVERED_READER:
        return o << "DISCOVERED_READER";
    case DS::CHANGED_QOS_READER:
        return o << "CHANGED_QOS_READER";
    case DS::REMOVED_READER:
        return o << "REMOVED_READER";
    default:    // unknown value, error
        o.setstate(std::ios::failbit);
    }

    return o;
}

std::ostream& eprosima::discovery_server::operator<<(std::ostream& o, WriterDiscoveryInfo::DISCOVERY_STATUS s)
{
    typedef WriterDiscoveryInfo::DISCOVERY_STATUS DS;

    switch (s)
    {
    case DS::DISCOVERED_WRITER:
        return o << "DISCOVERED_WRITER";
    case DS::CHANGED_QOS_WRITER:
        return o << "CHANGED_QOS_WRITER";
    case DS::REMOVED_WRITER:
        return o << "REMOVED_WRITER";
    default:    // unknown value, error
        o.setstate(std::ios::failbit);
    }

    return o;
}


bool DSManager::allKnowEachOther() const
{
    // Get a copy of current state
    Snapshot shot = _state.GetState();
    return allKnowEachOther(shot);
}

Snapshot& DSManager::takeSnapshot(const std::chrono::steady_clock::time_point tp, std::string & desc/* = std::string()*/)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);

    _snapshots.push_back(_state.GetState());

   Snapshot & shot = _snapshots.back();
   shot._time = tp;
   shot._des = desc;

   return shot;
}

/*static*/ 
bool DSManager::allKnowEachOther(const Snapshot & shot)
{
    // traverse snapshot comparing each member with each other
    Snapshot::const_iterator it1, it2;
    it2 = it1 = shot.cbegin();
    ++it2;

    while (it2 != shot.cend() && *it1 == *it2)
    {
        it1 = it2;
        ++it2;
    }

    return it2 == shot.cend();

}

bool DSManager::validateAllSnapshots() const
{
    // traverse the list of snapshots validating then
    bool work_it_all = true;
    
    for (const Snapshot &sh : _snapshots)
    {
        if (DSManager::allKnowEachOther(sh))
        {
            LOG_INFO(sh)
        }
        else
        {
            work_it_all = false;
            LOG_ERROR(sh);
        }
    }

    return work_it_all;
}