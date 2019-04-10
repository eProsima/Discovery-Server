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

DSManager::DSManager(const std::string &xml_file_path)
    : _active(false), _nocallbacks(false)
{
    //Log::SetVerbosity(Log::Warning);

    {   // fill in default topic attributes
        TopicAttributes & t = _defaultTopic;
        t.topicKind = NO_KEY;
        t.topicDataType = "HelloWorld";
        t.topicName = "HelloWorldTopic";
        t.historyQos.kind = KEEP_LAST_HISTORY_QOS;
        t.historyQos.depth = 30;
        t.resourceLimitsQos.max_samples = 50;
        t.resourceLimitsQos.allocated_samples = 20;
    }

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(xml_file_path.c_str()) == tinyxml2::XMLError::XML_SUCCESS)
    {
        tinyxml2::XMLElement *root = doc.FirstChildElement(s_sDS.c_str());
        if (!root)
        {
            LOG("Invalid config file");
            return;
        }

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

void DSManager::addSubscriber(Subscriber * sub)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);
    assert(_subs[sub->getGuid()] == nullptr);
    _subs[sub->getGuid()] = sub;
}

void DSManager::addPublisher(Publisher * pub)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);
    assert(_pubs[pub->getGuid()] == nullptr);
    _pubs[pub->getGuid()] = pub;
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

    // now we create the new participant
    Participant * pServer = Domain::createParticipant(atts,this);

    if (!pServer)
    {
        LOG_ERROR("DSManager couldn't create the server " << prefix << " with profile " << profile_name);
        return;
    }

    addServer(pServer);

    // Once the participant is created we create the associated endpoints
    tinyxml2::XMLElement* pub = server->FirstChildElement(xmlparser::PUBLISHER);
    while (pub)
    {
        loadPublisher(pServer,pub);
        pub = server->NextSiblingElement(xmlparser::PUBLISHER);
    }

    tinyxml2::XMLElement* sub = server->FirstChildElement(xmlparser::SUBSCRIBER);
    while (sub)
    {
        loadSubscriber(pServer,sub);
        sub = server->NextSiblingElement(xmlparser::SUBSCRIBER);
    }
}


void DSManager::loadClient(tinyxml2::XMLElement* client)
{
    std::lock_guard<std::recursive_mutex> lock(_mtx);

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

    // now we create the new participant
    Participant * pClient = Domain::createParticipant(atts,this);

    if (!pClient)
    {
        LOG_ERROR("DSManager couldn't create a client with profile " << profile_name);
        return;
    }

    addClient(pClient);

    // Once the participant is created we create the associated endpoints
    tinyxml2::XMLElement* pub = client->FirstChildElement(xmlparser::PUBLISHER);
    while (pub)
    {
        loadPublisher(pClient,pub);
        pub = client->NextSiblingElement(xmlparser::PUBLISHER);
    }

    tinyxml2::XMLElement* sub = client->FirstChildElement(xmlparser::SUBSCRIBER);
    while (sub)
    {
        loadSubscriber(pClient,sub);
        sub = client->NextSiblingElement(xmlparser::SUBSCRIBER);
    }

}

void DSManager::loadSubscriber(Participant * part, tinyxml2::XMLElement* sub)
{
    assert(part != nullptr && sub != nullptr);

    std::lock_guard<std::recursive_mutex> lock(_mtx);

    // subscribers are created for debugging purposes
    // default topic is the static HelloWorld one
    const char * profile_name = sub->Attribute(xmlparser::PROFILE_NAME);

    SubscriberAttributes subatts;

    if (!profile_name)
    {
        // get default subscriber attributes
        xmlparser::XMLProfileManager::getDefaultSubscriberAttributes(subatts);
    }
    else
    {
        // try load from profile
        if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::fillSubscriberAttributes(std::string(profile_name), subatts))
        {
            LOG_ERROR("DSManager::loadSubscriber couldn't load profile " << profile_name);
            return;
        }
    }

    // see if topic is specified
    const char * topic_name = sub->Attribute(xmlparser::TOPIC);

    if (topic_name)
    {
        if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::fillTopicAttributes(std::string(topic_name), subatts.topic))
        {
            LOG_ERROR("DSManager::loadSubscriber couldn't load topic profile " << profile_name);
            return;
        }
    }

    // check if we have topic info
    if (subatts.topic.getTopicName() == "UNDEF")
    {
        // fill in default topic
        subatts.topic = _defaultTopic;

        // assure the participant has default type registered
        TopicDataType* pT = nullptr;
        if (!Domain::getRegisteredType(part, _defaultType.getName(), &pT))
        {
            Domain::registerType(part, &_defaultType);
        }
    }
    else
    {
        // assure the participant has the type registered
        TopicDataType* pT = nullptr;
        std::string type_name = subatts.topic.getTopicDataType();
        if (!Domain::getRegisteredType(part, type_name.c_str() , &pT))
        {
            // Create dynamic type
            types::DynamicPubSubType* pDt = xmlparser::XMLProfileManager::CreateDynamicPubSubType(type_name);
            if (!pDt)
            {
                LOG_ERROR("DSManager::loadSubscriber couldn't create type " << type_name);
                return;
            }

            // register it
            _types[type_name] = pDt;
            // Domain::registerDynamicType(part, pDt);
            Domain::registerType(part, pDt);
        }

    }

    // Create the subscriber, listener doesn't report discovery info but matching one
    Subscriber * pSubs = Domain::createSubscriber(part, subatts, nullptr);

    if (!pSubs)
    {
        LOG_ERROR("DSManager couldn't create a subscriber with profile " << profile_name);
        return;
    }

    addSubscriber(pSubs);
}

void DSManager::loadPublisher(Participant * part, tinyxml2::XMLElement* sub)
{
    assert(part != nullptr && sub != nullptr);

    std::lock_guard<std::recursive_mutex> lock(_mtx);

    // subscribers are created for debugging purposes
    // default topic is the static HelloWorld one
    const char * profile_name = sub->Attribute(xmlparser::PROFILE_NAME);

    PublisherAttributes pubatts;

    if (!profile_name)
    {
        // get default subscriber attributes
        xmlparser::XMLProfileManager::getDefaultPublisherAttributes(pubatts);
    }
    else
    {
        // try load from profile
        if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::fillPublisherAttributes(std::string(profile_name), pubatts))
        {
            LOG_ERROR("DSManager::loadPublisher couldn't load profile " << profile_name);
            return;
        }
    }

    // see if topic is specified
    const char * topic_name = sub->Attribute(xmlparser::TOPIC);

    if (topic_name)
    {
        if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::fillTopicAttributes(std::string(topic_name), pubatts.topic))
        {
            LOG_ERROR("DSManager::loadPublisher couldn't load topic profile ");
            return;
        }
    }

    // check if we have topic info
    if (pubatts.topic.getTopicName() == "UNDEF")
    {
        // fill in default topic
        pubatts.topic = _defaultTopic;

        // assure the participant has default type registered
        TopicDataType* pT = nullptr;
        if (!Domain::getRegisteredType(part, _defaultType.getName(), &pT))
        {
            Domain::registerType(part, &_defaultType);
        }
    }
    else
    {
        // assure the participant has the type registered
        TopicDataType* pT = nullptr;
        std::string type_name = pubatts.topic.getTopicDataType();
        if (!Domain::getRegisteredType(part, type_name.c_str(), &pT))
        {
            // Create dynamic type
            types::DynamicPubSubType* pDt = xmlparser::XMLProfileManager::CreateDynamicPubSubType(type_name);
            if (!pDt)
            {
                LOG_ERROR("DSManager::loadPublisher couldn't create type " << type_name);
                return;
            }

            // register it
            _types[type_name] = pDt;
            // Domain::registerDynamicType(part, pDt);
            Domain::registerType(part, pDt);
        }

    }

    // Create the subscriber, listener doesn't report discovery info but matching one
    Publisher * pPubs = Domain::createPublisher(part, pubatts, nullptr);

    if (!pPubs)
    {
        LOG_ERROR("DSManager couldn't create a subscriber with profile " << profile_name);
        return;
    }

    addPublisher(pPubs);
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
        part_name = static_cast<std::ostringstream&>(std::ostringstream() << partid).str();
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
        part_name = static_cast<std::ostringstream&>(std::ostringstream() << partid).str();
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


bool DSManager::allKnowEachOther()
{
    // Get a copy of current state
    Snapshot shot = _state.GetState();

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