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

#include <tinyxml2.h>

#include <fastrtps/Domain.h>
#include <fastrtps/xmlparser/XMLProfileManager.h>

#include <fastrtps/subscriber/Subscriber.h>
#include <fastrtps/publisher/Publisher.h>

#include <fastrtps/transport/TCPv4TransportDescriptor.h>
#include <fastrtps/transport/TCPv6TransportDescriptor.h>
#include <fastrtps/utils/IPLocator.h>

#include "DSManager.h"
#include "LJ.h"
#include "IDs.h"

#include <iostream>
#include <sstream>

using namespace eprosima::fastrtps;
using namespace eprosima::discovery_server;

// non exported from fast-RTPS (watch out they may be updated)
namespace eprosima {
namespace fastrtps {
namespace DSxmlparser {
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
HelloWorldPubSubType DSManager::builtin_defaultType;
TopicAttributes DSManager::builtin_defaultTopic("HelloWorldTopic", "HelloWorld");
const std::regex DSManager::ipv4_regular_expression("^((?:[0-9]{1,3}\\.){3}[0-9]{1,3})?:?(?:(\\d+))?$");
const std::chrono::seconds DSManager::last_snapshot_delay_ = std::chrono::seconds(1);

DSManager::DSManager(
    const std::string& xml_file_path)
    : no_callbacks(false)
    , auto_shutdown(false)
    , enable_prefix_validation(true)
    , last_PDP_callback_(Snapshot::_st_ck)
    , last_EDP_callback_(Snapshot::_st_ck)
{
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(xml_file_path.c_str()) == tinyxml2::XMLError::XML_SUCCESS)
    {
        tinyxml2::XMLElement* root = doc.FirstChildElement(s_sDS.c_str());
        if (root == nullptr)
        {
            root = doc.FirstChildElement(s_sDS_Snapshots.c_str());
            if (root == nullptr)
            {
                LOG("Invalid config or snapshot file");
                return;
            }
            else
            {
                loadSnapshots(xml_file_path);
                auto_shutdown = true;
                LOG("Loaded snapshot file");
                return;
            }
        }

        // try load the user_shutdown attribute
        auto_shutdown = !root->BoolAttribute(s_sUserShutdown.c_str(), !auto_shutdown);

        // try load the enable_prefix_validation attribute
        enable_prefix_validation = root->BoolAttribute(s_sPrefixValidation.c_str(), enable_prefix_validation);

        for (auto child = doc.FirstChildElement(s_sDS.c_str());
            child != nullptr; child = child->NextSiblingElement(s_sDS.c_str()))
        {
            // first make XMLProfileManager::loadProfiles parse the config file. Afterwards loaded info is accessible
            // through XMLProfileManager::fillParticipantAttributes and related

            tinyxml2::XMLElement* profiles = child->FirstChildElement(DSxmlparser::PROFILES);
            if (profiles != nullptr)
            {
                loadProfiles(profiles);
            }
            else
            {
                LOG_ERROR("No profiles found!");
                return;
            }

            // Types parsing
            tinyxml2::XMLElement* types = child->FirstChildElement(DSxmlparser::TYPES);
            if (types != nullptr)
            {
                if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::loadXMLDynamicTypes(*types))
                {
                    LOG_INFO("No dynamic type information loaded.");
                }
            }

            // Server processing requires a two pass analysis
            tinyxml2::XMLElement* servers = child->FirstChildElement(s_sServers.c_str());

            if (servers != nullptr)
            {
                // 1. First map each server with his locators
                tinyxml2::XMLElement* server = servers->FirstChildElement(s_sServer.c_str());
                while (server != nullptr)
                {
                    MapServerInfo(server);
                    server = server->NextSiblingElement(s_sServer.c_str());
                }

                // 2. Create the servers according with their configuration

                server = servers->FirstChildElement(s_sServer.c_str());
                while (server != nullptr)
                {
                    loadServer(server);
                    server = server->NextSiblingElement(s_sServer.c_str());
                }
            }
            else if (!auto_shutdown)
            {
                LOG_ERROR("No servers found!");
                return; // For testing purposes, don't return.
            }

            // Create the clients according with the configuration, clients have only testing purposes
            tinyxml2::XMLElement* clients = child->FirstChildElement(s_sClients.c_str());
            if (clients != nullptr)
            {
                tinyxml2::XMLElement* client = clients->FirstChildElement(s_sClient.c_str());
                while (client != nullptr)
                {
                    loadClient(client);
                    client = client->NextSiblingElement(s_sClient.c_str());
                }
            }

            // Create the simples according with the configuration, simples have only testing purposes
            tinyxml2::XMLElement* simples = child->FirstChildElement(s_sSimples.c_str());
            if(simples != nullptr)
            {
                tinyxml2::XMLElement* simple = simples->FirstChildElement(s_sSimple.c_str());
                while(simple != nullptr)
                {
                    loadSimple(simple);
                    simple = simple->NextSiblingElement(s_sSimple.c_str());
                }
            }

            // Create snapshot events
            tinyxml2::XMLElement* snapshots = child->FirstChildElement(s_sSnapshots.c_str());
            if (snapshots)
            {
                const char* file = snapshots->Attribute(s_sFile.c_str());
                if (file != nullptr)
                {
                    snapshots_output_file = file;
                }
                tinyxml2::XMLElement* snapshot = snapshots->FirstChildElement(s_sSnapshot.c_str());
                while (snapshot != nullptr)
                {
                    loadSnapshot(snapshot);
                    snapshot = snapshot->NextSiblingElement(s_sSnapshot.c_str());
                }
            }
        }
    }
    else
    {
        LOG("Config file not found.");
    }

    LOG_INFO("File " << xml_file_path << " parsed successfully.");
}

DSManager::DSManager(
        const std::set<std::string>& xml_snapshot_files)
    : no_callbacks(true)
    , auto_shutdown(true)
    , enable_prefix_validation(true)
    , last_PDP_callback_(Snapshot::_st_ck)
    , last_EDP_callback_(Snapshot::_st_ck)
{
    for (const std::string& file : xml_snapshot_files)
    {
        if(loadSnapshots(file))
        {
            LOG("Loaded snapshot file " << file);
        }
    }
}

void DSManager::runEvents(
        std::istream& in /*= std::cin*/,
        std::ostream& out /*= std::cout*/)
{
    // Order the event list
    std::sort(events.begin(), events.end(), [](LJD* p1, LJD* p2) -> bool { return *p1 < *p2; });

    // traverse the list
    for (LJD* p : events)
    {
        // Wait till specified time
        p->Wait();
        // execute
        (*p)(*this);
    }

    // multiple processes sync delay
    if(!snapshots_output_file.empty())
    {
        std::this_thread::sleep_for(last_snapshot_delay_);
    }

    // Wait for user shutdown
    if (!auto_shutdown)
    {
        out << "\n### Discovery Server is running, press any key to quit ###" << std::endl;
        out.flush();
        in.ignore();
    }
}

void DSManager::addServer(
        Participant* s)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    assert(servers[s->getGuid()] == nullptr);
    servers[s->getGuid()] = s;
}

void DSManager::addClient(
        Participant* c)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    assert(clients[c->getGuid()] == nullptr);
    clients[c->getGuid()] = c;
}

void DSManager::addSimple(
    Participant* s)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    assert(simples[s->getGuid()] == nullptr);
    simples[s->getGuid()] = s;
}

Participant* DSManager::getParticipant(
        GUID_t& id)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    // first in clients
    participant_map::iterator it = clients.find(id);
    if (it != clients.end())
    {
        return it->second;
    }
    else if ((it = servers.find(id)) != servers.end())
    {
        return it->second;
    }
    else if((it = simples.find(id)) != simples.end())
    {
        return it->second;
    }

    return nullptr;
}

Participant* DSManager::removeParticipant(
        GUID_t& id)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    Participant * ret = nullptr;

    // update database
    state.RemoveParticipant(id);

    // remove any related pubs-subs
    {
        publisher_map paux;
        std::remove_copy_if(publishers.begin(), publishers.end(), std::inserter(paux, paux.begin()),
            [&id](publisher_map::value_type it)
            { 
                return id.guidPrefix == it.first.guidPrefix;
            });
        publishers.swap(paux);

        subscriber_map saux;
        std::remove_copy_if(subscribers.begin(), subscribers.end(), std::inserter(saux, saux.begin()),
            [&id](subscriber_map::value_type it)
            { 
                return id.guidPrefix == it.first.guidPrefix;
            });
        subscribers.swap(saux);
    }

    // first in clients
    participant_map::iterator it = clients.find(id);
    if (it != clients.end())
    {
        ret = it->second;
        clients.erase(it);
    }
    else if ((it = servers.find(id)) != servers.end())
    {
        ret = it->second;
        servers.erase(it);
    }
    else if((it = simples.find(id)) != simples.end())
    {
        ret = it->second;
        simples.erase(it);
    }

    return ret;
}

void DSManager::addSubscriber(
        Subscriber* sub)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    assert(subscribers[sub->getGuid()] == nullptr);
    subscribers[sub->getGuid()] = sub;
}

Subscriber* DSManager::getSubscriber(
        GUID_t& id)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    subscriber_map::iterator it = subscribers.find(id);
    if (it != subscribers.end())
    {
        return it->second;
    }

    return nullptr;
}

Subscriber* DSManager::removeSubscriber(
        GUID_t& id)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    Subscriber * ret = nullptr;

    subscriber_map::iterator it = subscribers.find(id);
    if (it != subscribers.end())
    {
        ret = it->second;
        subscribers.erase(it);
    }

    return ret;
}

void DSManager::addPublisher(
        Publisher* pub)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    assert(publishers[pub->getGuid()] == nullptr);
    publishers[pub->getGuid()] = pub;
}

Publisher* DSManager::getPublisher(
        GUID_t& id)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    publisher_map::iterator it = publishers.find(id);
    if (it != publishers.end())
    {
        return it->second;
    }

    return nullptr;
}

Publisher* DSManager::removePublisher(
        GUID_t& id)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    Publisher * ret = nullptr;

    publisher_map::iterator it = publishers.find(id);
    if (it != publishers.end())
    {
        ret = it->second;
        publishers.erase(it);
    }

    return ret;
}

types::DynamicPubSubType* DSManager::getType(
        std::string& name)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    type_map::iterator it = loaded_types.find(name);
    if (it != loaded_types.end())
    {
        return it->second;
    }

    return nullptr;
}

types::DynamicPubSubType* DSManager::setType(
        std::string& type_name)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    // Create dynamic type
    types::DynamicPubSubType* & pDt = loaded_types[type_name];

    if (pDt == nullptr)
    {
        pDt = xmlparser::XMLProfileManager::CreateDynamicPubSubType(type_name);
        if (pDt == nullptr)
        {
            loaded_types.erase(type_name);

            LOG_ERROR("DSManager::setType couldn't create type " << type_name);
            return nullptr;
        }
    }

    return pDt;
}

void DSManager::loadProfiles(
        tinyxml2::XMLElement* profiles)
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
        std::lock_guard<std::recursive_mutex> lock(management_mutex);

        if (no_callbacks)
        {
            return; // DSManager already terminated
        }

        no_callbacks = true;
    }

    // release all subscribers
    for (const auto&s : subscribers)
    {
        Domain::removeSubscriber(s.second);
    }

    subscribers.clear();

    // release all publishers
    for (const auto&p : publishers)
    {
        Domain::removePublisher(p.second);
    }

    publishers.clear();

    clients.insert(simples.begin(), simples.end());
    // the servers are appended because they should be destroyed at the end
    clients.insert(servers.begin(), servers.end());

    for (const auto&e : clients)
    {
        Participant *p = e.second;
        if (p)
        {
            Domain::removeParticipant(p);
        }
    }

    servers.clear();
    clients.clear();
    simples.clear();

    //unregister the types
    for (const auto& t : loaded_types)
    {
        xmlparser::XMLProfileManager::DeleteDynamicPubSubType(t.second);
    }

    loaded_types.clear();

    // remove all events
    for (auto ptr : events)
    {
        delete ptr;
    }

    events.clear();
}

DSManager::~DSManager()
{
    if (!snapshots_output_file.empty())
    {
        saveSnapshots(snapshots_output_file);
    }
    onTerminate();
}

void DSManager::loadServer(
        tinyxml2::XMLElement* server)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    // check if we need to create an event
    std::chrono::steady_clock::time_point creation_time, removal_time;
    creation_time = removal_time = getTime();

    {
        const char* creation_time_str = server->Attribute(s_sCreationTime.c_str());
        if (creation_time_str != nullptr)
        {
            int aux;
            std::istringstream(creation_time_str) >> aux;
            creation_time += std::chrono::seconds(aux);
        }

        const char* removal_time_str = server->Attribute(s_sRemovalTime.c_str());
        if (removal_time_str != nullptr)
        {
            int aux;
            std::istringstream(removal_time_str) >> aux;
            removal_time += std::chrono::seconds(aux);
        }
    }

    // profile name is mandatory
    const char* profile_name = server->Attribute(DSxmlparser::PROFILE_NAME);

    if (profile_name == nullptr || *profile_name == '\0')
    {
        std::stringstream msg;
        msg << DSxmlparser::PROFILE_NAME << " is a mandatory attribute of server tag";

        if (profile_name != nullptr)
        {    // may be empty on purpose (for creating dummie clients)
            LOG_INFO(msg.str());
        }
        else
        {   
            LOG_ERROR(msg.str());
        }
        
        return;
    }

    // retrieve profile attributes
    ParticipantAttributes atts;
    if (xmlparser::XMLP_ret::XML_OK != 
        xmlparser::XMLProfileManager::fillParticipantAttributes(std::string(profile_name), atts))
    {
        LOG_ERROR("DSManager::loadServer couldn't load profile " << profile_name);
        return;
    }

    // server name is either pass as an attribute (preferred to allow profile reuse) or inside the profile
    const char* name = server->Attribute(DSxmlparser::NAME);
    if (name != nullptr)
    {
        atts.rtps.setName(name);
    }

    // server GuidPrefix is either pass as an attribute (preferred to allow profile reuse)
    // or inside the profile.
    GuidPrefix_t& prefix = atts.rtps.prefix;
    const char* cprefix = server->Attribute(DSxmlparser::PREFIX);

    if (cprefix != nullptr &&
        !(std::istringstream(cprefix) >> prefix) &&
        (prefix == c_GuidPrefix_Unknown))
    {
        LOG_ERROR("Servers cannot have a framework provided prefix"); // at least for now
        return;
    }

    GUID_t guid(prefix, c_EntityId_RTPSParticipant);

    // Check if the guidPrefix is already in use (there is a mistake on config file)
    if (enable_prefix_validation &&
        servers.find(guid) != servers.end())
    {
        LOG_ERROR("DSManager detected two servers sharing the same prefix " << prefix);
        return;
    }

    // replace the atts.rtps.builtin lists with the ones from server_locators (if present)
    // note that a previous call to DSManager::MapServerInfo
    serverLocator_map::mapped_type & lists = server_locators[guid];
    if (!lists.first.empty() ||
        !lists.second.empty())
    {
        // server elements take precedence over profile ones
        // I copy them because other servers may need this values
        atts.rtps.builtin.metatrafficMulticastLocatorList = lists.first;
        atts.rtps.builtin.metatrafficUnicastLocatorList = lists.second;
    }

    // load the server list (if present) and update the atts.rtps.builtin
    tinyxml2::XMLElement* server_list = server->FirstChildElement(s_sSL.c_str());

    if (server_list != nullptr)
    {
        RemoteServerList_t& list = atts.rtps.builtin.discovery_config.m_DiscoveryServers;
        list.clear(); // server elements take precedence over profile ones

        tinyxml2::XMLElement * rserver = server_list->FirstChildElement(s_sRServer.c_str());

        while (rserver != nullptr)
        {
            RemoteServerList_t::value_type srv;
            GuidPrefix_t& prefix = srv.guidPrefix;

            // load the prefix
            const char* cprefix = rserver->Attribute(DSxmlparser::PREFIX);

            if (cprefix != nullptr &&
                !(std::istringstream(cprefix) >> prefix)
                && (prefix == c_GuidPrefix_Unknown))
            {
                LOG_ERROR("RServers must provide a prefix"); // at least for now
                return;
            }

            // load the locator lists
            serverLocator_map::mapped_type & lists = server_locators[srv.GetParticipant()];
            srv.metatrafficMulticastLocatorList = lists.first;
            srv.metatrafficUnicastLocatorList = lists.second;

            list.push_back(std::move(srv));

            rserver = rserver->NextSiblingElement(s_sRServer.c_str());
        }
    }

    // We define the PDP as external (when moved to fast library it would be SERVER)
    DiscoverySettings & b = atts.rtps.builtin.discovery_config;
    (void)b;
    assert(b.discoveryProtocol == SERVER || b.discoveryProtocol == BACKUP);

    // Create the participant or the associated events
    DPC event(creation_time, std::move(atts), &DSManager::addServer);

    if (creation_time == getTime())
    {
        event(*this);
    }
    else
    {   // late joiner
        events.push_back(new DPC(std::move(event)));
    }

    if (removal_time != getTime())
    {
        // early leaver
        events.push_back(new DPD(removal_time, guid));
    }

    // Once the participant is created we create the associated endpoints
    tinyxml2::XMLElement* pub = server->FirstChildElement(DSxmlparser::PUBLISHER);
    while (pub != nullptr)
    {
        loadPublisher(guid, pub);
        pub = pub->NextSiblingElement(DSxmlparser::PUBLISHER);
    }

    tinyxml2::XMLElement* sub = server->FirstChildElement(DSxmlparser::SUBSCRIBER);
    while (sub != nullptr)
    {
        loadSubscriber(guid, sub);
        sub = sub->NextSiblingElement(DSxmlparser::SUBSCRIBER);
    }
}

void DSManager::loadClient(
        tinyxml2::XMLElement* client)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    // check if we need to create an event
    std::chrono::steady_clock::time_point creation_time, removal_time;
    creation_time = removal_time = getTime();

    {
        const char* creation_time_str = client->Attribute(s_sCreationTime.c_str());
        if (creation_time_str != nullptr)
        {
            int aux;
            std::istringstream(creation_time_str) >> aux;
            creation_time += std::chrono::seconds(aux);
        }

        const char* removal_time_str = client->Attribute(s_sRemovalTime.c_str());
        if (removal_time_str != nullptr)
        {
            int aux;
            std::istringstream(removal_time_str) >> aux;
            removal_time += std::chrono::seconds(aux);
        }
    }

    // clients are created for debugging purposes
    // profile name is mandatory because they must reference servers
    const char* profile_name = client->Attribute(DSxmlparser::PROFILE_NAME);

    if (profile_name == nullptr)
    {
        LOG_ERROR(DSxmlparser::PROFILE_NAME << " is a mandatory attribute of client tag");
        return;
    }

    // retrieve profile attributes
    ParticipantAttributes atts;
    if (xmlparser::XMLP_ret::XML_OK != 
        xmlparser::XMLProfileManager::fillParticipantAttributes(std::string(profile_name), atts))
    {
        LOG_ERROR("DSManager::loadClient couldn't load profile " << profile_name);
        return;
    }

    // we must assert that DiscoveryProtocol is CLIENT
    if (atts.rtps.builtin.discovery_config.discoveryProtocol != rtps::DiscoveryProtocol_t::CLIENT)
    {
        LOG_ERROR("DSManager::loadClient try to create a client with an incompatible profile: " << profile_name);
        return;
    }

    // pick the client's name (isn't mandatory to provide it). Takes precedence over profile provided.
    const char* name = client->Attribute(DSxmlparser::NAME);
    if (name != nullptr)
    {
        atts.rtps.setName(name);
    }

    // server may be provided by prefix (takes precedence) or by list
    const char* server = client->Attribute(s_sServer.c_str());
    if (server != nullptr)
    {
        RemoteServerList_t::value_type srv;
        GuidPrefix_t & prefix = srv.guidPrefix;

        if (!(std::istringstream(server) >> prefix) && 
            (prefix == c_GuidPrefix_Unknown))
        {
            LOG_ERROR("server attribute must provide a prefix"); // at least for now
            return;
        }

        RemoteServerList_t & list = atts.rtps.builtin.discovery_config.m_DiscoveryServers;
        list.clear(); // server elements take precedence over profile ones

        // load the locator lists
        serverLocator_map::mapped_type & lists = server_locators[srv.GetParticipant()];
        srv.metatrafficMulticastLocatorList = lists.first;
        srv.metatrafficUnicastLocatorList = lists.second;

        list.push_back(std::move(srv));
    }
    else
    {
        // load the server list (if present) and update the atts.rtps.builtin
        tinyxml2::XMLElement *server_list = client->FirstChildElement(s_sSL.c_str());

        if (server_list != nullptr)
        {
            RemoteServerList_t & list = atts.rtps.builtin.discovery_config.m_DiscoveryServers;
            list.clear(); // server elements take precedence over profile ones

            tinyxml2::XMLElement * rserver = server_list->FirstChildElement(s_sRServer.c_str());

            while (rserver != nullptr)
            {
                RemoteServerList_t::value_type srv;
                GuidPrefix_t & prefix = srv.guidPrefix;

                // load the prefix
                const char * cprefix = rserver->Attribute(DSxmlparser::PREFIX);

                if (cprefix != nullptr && !(std::istringstream(cprefix) >> prefix)
                    && (prefix == c_GuidPrefix_Unknown))
                {
                    LOG_ERROR("RServers must provide a prefix"); // at least for now
                    return;
                }

                // load the locator lists
                serverLocator_map::mapped_type & lists = server_locators[srv.GetParticipant()];
                srv.metatrafficMulticastLocatorList = lists.first;
                srv.metatrafficUnicastLocatorList = lists.second;

                list.push_back(std::move(srv));

                rserver = rserver->NextSiblingElement(s_sRServer.c_str());
            }
        }
    }

    // check for listening ports
    const char* lp = client->Attribute(s_sListeningPort.c_str());
    if (lp != nullptr)
    {
        // parse the expression, listening ports only have sense on TCP
        long listening_port;
        std::cmatch mr;
        std::string address; // WAN address, IPv4 address saw from the outside

        if (std::regex_match(lp, mr, ipv4_regular_expression))
        {
            listening_port = std::stol(mr[2].str());
            
            // if address is empty ipv4_regular_expression matches a port number but we don't know the kind of transport
            address = mr[1].str();
        }
        else
        {
            LOG_ERROR("invalid listening port for server (" << atts.rtps.getName() << ")"); // at least for now
            return;
        }

        // Look for a suitable user transport
        TCPTransportDescriptor * pT = nullptr;
        std::shared_ptr<TCPv4TransportDescriptor> p4;
        std::shared_ptr<TCPv6TransportDescriptor> p6;

        // Supossed to be only one transport of each class
        for (auto sp : atts.rtps.userTransports)
        {
            pT = dynamic_cast<TCPTransportDescriptor*>(sp.get());

            if (pT != nullptr)
            {
                if (!p4)
                {   // try to find a descriptor matching the listener port setup
                    if (p4 = std::dynamic_pointer_cast<TCPv4TransportDescriptor>(sp))
                    {
                        continue;
                    }
                }

                if (!p6)
                {   // try to find a descriptor matching the listener port setup
                    p6 = std::dynamic_pointer_cast<TCPv6TransportDescriptor>(sp);
                }
            }
        }

        if ((address.empty() && !p4 && !p6) || (!address.empty() && !p4))
        {
            // create a new tcp4 one
            p4 = std::make_shared<TCPv4TransportDescriptor>();
            pT = p4.get();
            
            // update participant attributes
            atts.rtps.userTransports.push_back(p4);
        }
        else if (p4)
        {
            // create a copy of a tcp4 and replace in transports
            for (std::shared_ptr<TransportDescriptorInterface> & sp : atts.rtps.userTransports)
            {
                if (p4 == sp)
                {
                    p4 = std::make_shared<TCPv4TransportDescriptor>(*p4);
                    pT = p4.get();
                    sp = std::dynamic_pointer_cast<TransportDescriptorInterface>(p4);
                }
            }
        }
        else
        {
            // create a copy of a tcp6 and replace in transports
            for (std::shared_ptr<TransportDescriptorInterface> & sp : atts.rtps.userTransports)
            {
                if (p6 == sp)
                {
                    p6 = std::make_shared<TCPv6TransportDescriptor>(*p6);
                    pT = p6.get();
                    sp = std::dynamic_pointer_cast<TransportDescriptorInterface>(p6);
                }
            }
        }

        pT->add_listener_port(static_cast<uint16_t>(listening_port));

        // set up WAN if specified
        if (!address.empty() && p4)
        {
            p4->set_WAN_address(address);
        }

    }

    GUID_t guid(atts.rtps.prefix, c_EntityId_RTPSParticipant);
    DPD* pD = nullptr;
    DPC* pC = nullptr;

    if (removal_time != getTime())
    {
        // early leaver
        pD = new DPD(removal_time, guid);
        events.push_back(pD);
    }

    // Create the participant or the associated events
    DPC event(creation_time, std::move(atts), &DSManager::addClient, pD);

    if (creation_time == getTime())
    {
        event(*this);

        // get the client guid
        guid = event.participant_guid;
    }
    else
    {   // late joiner
        pC = new DPC(std::move(event));
        events.push_back(pC);
    }

    // Once the participant is created we create the associated endpoints
    tinyxml2::XMLElement* pub = client->FirstChildElement(DSxmlparser::PUBLISHER);
    while (pub != nullptr)
    {
        loadPublisher(guid, pub, pC);
        pub = pub->NextSiblingElement(DSxmlparser::PUBLISHER);
    }

    tinyxml2::XMLElement* sub = client->FirstChildElement(DSxmlparser::SUBSCRIBER);
    while (sub != nullptr)
    {
        loadSubscriber(guid, sub, pC);
        sub = sub->NextSiblingElement(DSxmlparser::SUBSCRIBER);
    }
}

void DSManager::loadSubscriber(
        GUID_t& part_guid, tinyxml2::XMLElement* sub,
        DPC* pLJ /*= nullptrt*/)
{
    assert(sub != nullptr);

    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    // retrieve participant
    Participant* part = getParticipant(part_guid);

    // check if we need to create an event
    std::chrono::steady_clock::time_point creation_time, removal_time;
    creation_time = removal_time = getTime();

    {
        const char* creation_time_str = sub->Attribute(s_sCreationTime.c_str());
        if (creation_time_str != nullptr)
        {
            int aux;
            std::istringstream(creation_time_str) >> aux;
            creation_time += std::chrono::seconds(aux);
        }
        else if (part == nullptr)
        {
            // late joiners must spawn late joiners
            LOG_ERROR("DSManager::loadSubscriber tries to create a subscriber to a late joiner.");
            return;
        }

        const char* removal_time_str = sub->Attribute(s_sRemovalTime.c_str());
        if (removal_time_str != nullptr)
        {
            int aux;
            std::istringstream(removal_time_str) >> aux;
            removal_time += std::chrono::seconds(aux);
        }
    }

    // subscribers are created for debugging purposes
    // default topic is the static HelloWorld one
    const char* profile_name = sub->Attribute(DSxmlparser::PROFILE_NAME);

    SubscriberAttributes * subatts = new SubscriberAttributes();

    if (profile_name == nullptr)
    {
        // get default subscriber attributes
        xmlparser::XMLProfileManager::getDefaultSubscriberAttributes(*subatts);
    }
    else
    {
        // try load from profile
        if (xmlparser::XMLP_ret::XML_OK !=
            xmlparser::XMLProfileManager::fillSubscriberAttributes(std::string(profile_name), *subatts))
        {
            LOG_ERROR("DSManager::loadSubscriber couldn't load profile " << profile_name);
            return;
        }
    }

    // see if topic is specified
    const char* topic_name = sub->Attribute(DSxmlparser::TOPIC);

    if (topic_name != nullptr)
    {
        if (xmlparser::XMLP_ret::XML_OK != 
            xmlparser::XMLProfileManager::fillTopicAttributes(std::string(topic_name), subatts->topic))
        {
            LOG_ERROR("DSManager::loadSubscriber couldn't load topic profile " << profile_name);
            return;
        }
    }

    DED<Subscriber>* pDE = nullptr; // subscriber destruction event

    if (removal_time != getTime())
    {
        // Destruction event needs the endpoint guid
        // that would be provided in creation
        pDE = new DED<Subscriber>(removal_time);
        events.push_back(pDE);
    }

    DEC<Subscriber> event(creation_time, subatts, part_guid, pDE, pLJ);

    if (creation_time == getTime())
    {
        event(*this);
    }
    else
    {   // late joiner
        events.push_back(new DEC<Subscriber>(std::move(event)));
    }
}

void DSManager::loadPublisher(
        GUID_t& part_guid, tinyxml2::XMLElement* sub,
        DPC* pLJ /*= nullptrt*/)
{
    assert(sub != nullptr);

    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    // retrieve participant
    Participant * part = getParticipant(part_guid);

    // check if we need to create an event
    std::chrono::steady_clock::time_point creation_time, removal_time;
    creation_time = removal_time = getTime();

    {
        const char * creation_time_str = sub->Attribute(s_sCreationTime.c_str());
        if (creation_time_str != nullptr)
        {
            int aux;
            std::istringstream(creation_time_str) >> aux;
            creation_time += std::chrono::seconds(aux);
        }
        else if (part == nullptr)
        {
            // late joiners must spawn late joiners
            LOG_ERROR("DSManager::loadSubscriber tries to create a subscriber to a late joiner.");
            return;
        }

        const char * removal_time_str = sub->Attribute(s_sRemovalTime.c_str());
        if (removal_time_str != nullptr)
        {
            int aux;
            std::istringstream(removal_time_str) >> aux;
            removal_time += std::chrono::seconds(aux);
        }
    }

    // subscribers are created for debugging purposes
    // default topic is the static HelloWorld one
    const char * profile_name = sub->Attribute(DSxmlparser::PROFILE_NAME);

    PublisherAttributes* pubatts = new PublisherAttributes();

    if (profile_name == nullptr)
    {
        // get default subscriber attributes
        xmlparser::XMLProfileManager::getDefaultPublisherAttributes(*pubatts);
    }
    else
    {
        // try load from profile
        if (xmlparser::XMLP_ret::XML_OK != 
            xmlparser::XMLProfileManager::fillPublisherAttributes(std::string(profile_name), *pubatts))
        {
            LOG_ERROR("DSManager::loadPublisher couldn't load profile " << profile_name);
            return;
        }
    }

    // see if topic is specified
    const char* topic_name = sub->Attribute(DSxmlparser::TOPIC);

    if (topic_name != nullptr)
    {
        if (xmlparser::XMLP_ret::XML_OK != 
            xmlparser::XMLProfileManager::fillTopicAttributes(std::string(topic_name), pubatts->topic))
        {
            LOG_ERROR("DSManager::loadPublisher couldn't load topic profile ");
            return;
        }
    }

    DED<Publisher>* pDE = nullptr; // subscriber destruction event

    if (removal_time != getTime())
    {
        // Destruction event needs the endpoint guid
        // that would be provided in creation
        pDE = new DED<Publisher>(removal_time);
        events.push_back(pDE);
    }

    DEC<Publisher> event(creation_time, pubatts, part_guid, pDE, pLJ);

    if (creation_time == getTime())
    {
        event(*this);
    }
    else
    {   // late joiner
        events.push_back(new DEC<Publisher>(std::move(event)));
    }
}

std::chrono::steady_clock::time_point DSManager::getTime() const
{
    return state.getTime();
}

void DSManager::loadSnapshot(
        tinyxml2::XMLElement* snapshot)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    // snapshots are created for debugging purposes
    // time is mandatory
    const char * time_str = snapshot->Attribute(s_sTime.c_str());

    if (time_str == nullptr)
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

    // fail if nobody is found?
    bool someone = snapshot->BoolAttribute(s_sSomeone.c_str(), true);

    // Get the description from the tag
    std::string description(snapshot->GetText());

    // Add the event
    events.push_back(new DS(time, description,someone));
}


void DSManager::MapServerInfo(
        tinyxml2::XMLElement* server)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    uint8_t ident = 1;

    // profile name is mandatory
    std::string profile_name(server->Attribute(DSxmlparser::PROFILE_NAME));

    if (profile_name.empty())
    {   // its doesn't log as error because may be empty on purpose
        LOG_INFO(DSxmlparser::PROFILE_NAME << " is a mandatory attribute of server tag");
        return;
    }

    // server GuidPrefix is either pass as an attribute (preferred to allow profile reuse)
    // or inside the profile.
    GuidPrefix_t prefix;
    std::shared_ptr<ParticipantAttributes> atts;

    const char* cprefix = server->Attribute(DSxmlparser::PREFIX);

    if (cprefix != nullptr)
    {
        std::istringstream(cprefix) >> prefix;
    }
    else
    {
        // I must load the prefix from the profile
        // retrieve profile attributes
        atts = std::make_shared<ParticipantAttributes>();
        if (xmlparser::XMLP_ret::XML_OK !=
            xmlparser::XMLProfileManager::fillParticipantAttributes(profile_name, *atts))
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

    tinyxml2::XMLElement* LP = server->FirstChildElement(s_sLP.c_str());
    if (LP != nullptr)
    {
        tinyxml2::XMLElement * list = LP->FirstChildElement(DSxmlparser::META_MULTI_LOC_LIST);

        if (list != nullptr &&
            (xmlparser::XMLP_ret::XML_OK != getXMLLocatorList(list, pair.first, ident)))
        {
            LOG_ERROR("Server " << prefix << " has an ill formed " << DSxmlparser::META_MULTI_LOC_LIST);
        }

        list = LP->FirstChildElement(DSxmlparser::META_UNI_LOC_LIST);
        if (list != nullptr &&
            (xmlparser::XMLP_ret::XML_OK != getXMLLocatorList(list, pair.second, ident)))
        {
            LOG_ERROR("Server " << prefix << " has an ill formed " << DSxmlparser::META_UNI_LOC_LIST);
        }

    }
    else
    {
        LocatorList_t multicast, unicast;

        // retrieve profile attributes
        if (!atts)
        {
            atts = std::make_shared<ParticipantAttributes>();
            if (xmlparser::XMLP_ret::XML_OK != 
                xmlparser::XMLProfileManager::fillParticipantAttributes(profile_name, *atts))
            {
                LOG_ERROR("DSManager::loadServer couldn't load profile " << profile_name);
                return;
            }
        }

        pair.first = atts->rtps.builtin.metatrafficMulticastLocatorList;
        pair.second = atts->rtps.builtin.metatrafficUnicastLocatorList;
    }

    // now save the value
    server_locators[GUID_t(prefix, c_EntityId_RTPSParticipant)] = std::move(pair);
}

void DSManager::onParticipantDiscovery(
        Participant* participant,
        rtps::ParticipantDiscoveryInfo&& info)
{
    bool server = false;
    const GUID_t& partid = info.info.m_guid;

    // update last_callback time
    last_PDP_callback_ = std::chrono::steady_clock::now();

    LOG_INFO("Participant " << participant->getAttributes().rtps.getName() << " reports a participant "
        << info.info.m_participantName << " is " << info.status << ". Prefix " << partid);

    {
        std::lock_guard<std::recursive_mutex> lock(management_mutex);

        if (!no_callbacks)
        {
            // DSManager info still valid
            server = servers.end() != servers.find(partid);
        }
    }

    // add to database, it has its own mtx
    // note that when a participant is destroyed he will wait for all his callbacks to return
    // state will be alive during all callbacks
    switch (info.status)
    {
    case ParticipantDiscoveryInfo::DISCOVERED_PARTICIPANT:
    {
        state.AddParticipant(participant->getGuid(), partid, info.info.m_participantName.to_string(), server);
        break;
    }
    case ParticipantDiscoveryInfo::REMOVED_PARTICIPANT:
    case ParticipantDiscoveryInfo::DROPPED_PARTICIPANT:
    {
        // only update the database if alive
        state.RemoveParticipant(participant->getGuid(), partid);
        break;
    }
    default:
        break;
    }

    // note that I ignore DROPPED_PARTICIPANT because it deals with liveliness
    // and not with discovery messages
}

void DSManager::onSubscriberDiscovery(
        Participant* participant,
        rtps::ReaderDiscoveryInfo&& info)
{
    // update last_callback time
    last_EDP_callback_ = std::chrono::steady_clock::now();

    typedef ReaderDiscoveryInfo::DISCOVERY_STATUS DS;

    const GUID_t & subsid = info.info.guid();
    GUID_t partid = iHandle2GUID(info.info.RTPSParticipantKey());

    // non reported info
    std::string part_name;

    {
        std::lock_guard<std::recursive_mutex> lock(management_mutex);

        participant_map::iterator it;

        if (!no_callbacks)
        {
            // is one of ours?
            if ((it = servers.find(partid)) != servers.end() ||
                (it = clients.find(partid)) != clients.end() ||
                (it = simples.find(partid)) != simples.end())
            {
                part_name = it->second->getAttributes().rtps.getName();
            }
        }
        else
        {   // stick to non-DSManager info
            for (const PtDI* p : state.FindParticipant(partid))
            {
                if (!p->participant_name.empty())
                {
                    part_name = p->participant_name;
                }
            }
        }
    }

    if (part_name.empty())
    {   // if remote use prefix instead of name
        part_name = std::ostringstream(std::ostringstream() << partid).str();
    }

    switch (info.status)
    {
    case DS::DISCOVERED_READER:
        state.AddSubscriber(participant->getGuid(), partid, subsid, info.info.typeName().to_string(),
            info.info.topicName().to_string());
        break;
    case DS::REMOVED_READER:
    {
        // only notify if participant is alive
        GUID_t guid;
        iHandle2GUID(guid, info.info.RTPSParticipantKey());
        if (getParticipant(guid))
        {
            state.RemoveSubscriber(participant->getGuid(), partid, subsid);
        }
    }
        break;
    default:
        break;
    }

    LOG_INFO("Participant " << participant->getAttributes().rtps.getName() << " reports a subscriber of participant "
        << part_name << " is " << info.status << " with typename: " << info.info.typeName()
        << " topic: " << info.info.topicName() << " GUID: " << subsid);
}

void  DSManager::onPublisherDiscovery(
        Participant* participant,
        rtps::WriterDiscoveryInfo&& info)
{
    // update last_callback time
    last_EDP_callback_ = std::chrono::steady_clock::now();

    typedef WriterDiscoveryInfo::DISCOVERY_STATUS DS;

    const GUID_t& pubsid = info.info.guid();
    GUID_t partid = iHandle2GUID(info.info.RTPSParticipantKey());

    // non reported info
    std::string part_name;

    {
        std::lock_guard<std::recursive_mutex> lock(management_mutex);

        if (!no_callbacks)
        {
            // is one of ours?
            participant_map::iterator it;

            if ((it = servers.find(partid)) != servers.end() ||
                (it = clients.find(partid)) != clients.end() ||
                (it = simples.find(partid)) != simples.end())
            {
                part_name = it->second->getAttributes().rtps.getName();
            }
        }
        else
        {   // stick to non-DSManager info
            for (const PtDI* p : state.FindParticipant(partid))
            {
                if (!p->participant_name.empty())
                {
                    part_name = p->participant_name;
                }
            }
        }
    }

    if (part_name.empty())
    {   // if remote use prefix instead of name
        part_name = std::ostringstream(std::ostringstream() << partid).str();
    }

    switch (info.status)
    {
    case DS::DISCOVERED_WRITER:
        state.AddPublisher(participant->getGuid(),
            partid, pubsid,
            info.info.typeName().to_string(),
            info.info.topicName().to_string());
        break;
    case DS::REMOVED_WRITER:
    {
        // only notify if participant is alive
        GUID_t guid;
        iHandle2GUID(guid, info.info.RTPSParticipantKey());
        if (getParticipant(guid))
        {
            state.RemovePublisher(participant->getGuid(), partid, pubsid);
        }
    }
        break;
    default:
        break;
    }

    LOG_INFO("Participant " << participant->getAttributes().rtps.getName() << " reports a publisher of participant "
        << part_name << " is " << info.status << " with typename: " << info.info.typeName()
        << " topic: " << info.info.topicName() << " GUID: " << pubsid);
}

std::ostream& eprosima::discovery_server::operator<<(
        std::ostream& o,
        ParticipantDiscoveryInfo::DISCOVERY_STATUS s)
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
        return o << "DROPPED_PARTICIPANT";
    default:    // unknown value, error
        o.setstate(std::ios::failbit);
    }

    return o;
}

std::ostream& eprosima::discovery_server::operator<<(
        std::ostream& o,
        ReaderDiscoveryInfo::DISCOVERY_STATUS s)
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

std::ostream& eprosima::discovery_server::operator<<(
        std::ostream& o,
        WriterDiscoveryInfo::DISCOVERY_STATUS s)
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
    Snapshot shot = state.GetState();
    return allKnowEachOther(shot);
}

Snapshot& DSManager::takeSnapshot(
        const std::chrono::steady_clock::time_point tp,
        const std::string& desc/* = std::string()*/,
        bool someone)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    snapshots.push_back(state.GetState());

    Snapshot& shot = snapshots.back();
    shot._time = tp;
    shot.last_PDP_callback_ = last_PDP_callback_;
    shot.last_EDP_callback_ = last_EDP_callback_;
    shot._des = desc;
    shot.if_someone = someone;

    // Add any simple, client or server isolated information 
    // those have not make any callbacks if no subscriber or publisher

    participant_map temp(servers);
    temp.insert(clients.begin(), clients.end());
    temp.insert(simples.begin(), simples.end());

    std::function<bool(const participant_map::value_type &, const Snapshot::value_type &)> pred(
        [](const participant_map::value_type & p1, const Snapshot::value_type & p2)
    {
        return p1.first == p2.endpoint_guid;
    }
    );

    std::pair<participant_map::const_iterator, Snapshot::const_iterator> res =
        std::mismatch(temp.cbegin(), temp.cend(), shot.cbegin(), shot.cend(), pred);

    while (res.first != temp.end())
    {
        // res.first participant hasn't any discovery info in this Snapshot
        res.second = shot.emplace_hint(res.second, PtDB(res.first->first));
        res = std::mismatch(res.first, temp.cend(), res.second, shot.cend(), pred);
    }

    return shot;
}

/*static*/
bool DSManager::allKnowEachOther(
        const Snapshot & shot)
{
    // nobody discovered is bad?
    if (shot.if_someone && shot.empty())
    {
        return false; // nobody out there
    }

    // traverse snapshot comparing each member with each other
    Snapshot::const_iterator it1, it2;
    it2 = it1 = shot.cbegin();

    if (it2 != shot.cend())
    {
        ++it2;
    }

    while (it2 != shot.cend() && *it1 == *it2)
    {
        it1 = it2;
        ++it2;
    }

    if (it2 != shot.cend())
    {
        LOG_ERROR("Failed checking:" << std::endl << *it1 << *it2);
    }

    return it2 == shot.cend();

}

bool DSManager::validateAllSnapshots() const
{
    // traverse the list of snapshots validating then
    bool work_it_all = true;

    for (const Snapshot& sh : snapshots)
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

bool DSManager::loadSnapshots(
        const std::string& file)
{
    using namespace tinyxml2;
    XMLDocument xmlDoc;

    if(tinyxml2::XML_SUCCESS != xmlDoc.LoadFile(file.c_str()))
    {
        LOG_ERROR("Couldn't parse the file: " << file);
        return false;
    }
    
    XMLNode * pRoot = xmlDoc.FirstChild();

    snapshots_list::iterator it;
    bool inserter = snapshots.empty();
    if (!inserter)
    {
        it = snapshots.begin();
    }

    for (XMLElement* pSh = pRoot->FirstChildElement(s_sDS_Snapshot.c_str());
        pSh != nullptr;
        pSh = pSh->NextSiblingElement(s_sDS_Snapshot.c_str()))
    {
        Snapshot sh;
        sh.from_xml(pSh);

        if (inserter)
        {
            snapshots.emplace_back(sh);
        }
        else
        {
            if (it == snapshots.end())
            {
                LOG_ERROR("Number of snapshots doesn't match: " << file);
                break;
            }
            *it++ += sh;
        }
    }

    return true;
}

void DSManager::saveSnapshots(
        const std::string& file) const
{
    using namespace tinyxml2;
    XMLDocument xmlDoc;
    XMLNode* pRoot = xmlDoc.NewElement(s_sDS_Snapshots.c_str());
    for (const Snapshot& sh : snapshots)
    {
        LOG("Saving snapshot " << sh._des);

        XMLElement* pShRoot = xmlDoc.NewElement(s_sDS_Snapshot.c_str());
        sh.to_xml(pShRoot, xmlDoc);
        pRoot->InsertEndChild(pShRoot);
    }
    xmlDoc.InsertEndChild(pRoot);
    XMLError error = xmlDoc.SaveFile(file.c_str());
    if (error)
    {
        LOG("Error while saving snapshot file " << file << ": " << error);
    }
    else
    {
        LOG("Snapshot file saved " << file << ".");
    }
}
