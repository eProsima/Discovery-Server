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

#include <iostream>
#include <sstream>

#include <tinyxml2.h>

#include <fastrtps/xmlparser/XMLProfileManager.h>

#include <fastdds/rtps/transport/UDPv4TransportDescriptor.h>
#include <fastdds/rtps/transport/TCPv4TransportDescriptor.h>
#include <fastdds/rtps/transport/TCPv6TransportDescriptor.h>

#include "DSManager.h"
#include "IDs.h"
#include "LJ.h"
#include "log/DSLog.h"


using namespace eprosima::fastrtps;
using namespace eprosima::fastdds;
using namespace eprosima::fastrtps::rtps;
using namespace eprosima::fastdds::rtps;
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
} // namespace DSxmlparser
} // namespace fastrtps
} // namespace eprosima

/*static members*/
TopicAttributes DSManager::builtin_defaultTopic("HelloWorldTopic", "HelloWorld");
const std::regex DSManager::ipv4_regular_expression("^((?:[0-9]{1,3}\\.){3}[0-9]{1,3})?:?(?:(\\d+))?$");
const std::chrono::seconds DSManager::last_snapshot_delay_ = std::chrono::seconds(1);

DSManager::DSManager(
        const std::string& xml_file_path,
        const bool shared_memory_off)
    : no_callbacks(false)
    , auto_shutdown(true)
    , enable_prefix_validation(true)
    , correctly_created_(false)
    , last_PDP_callback_(Snapshot::_steady_clock)
    , last_EDP_callback_(Snapshot::_steady_clock)
    , shared_memory_off_(shared_memory_off)
{
    tinyxml2::XMLDocument doc;
    if (tinyxml2::XMLError::XML_SUCCESS == doc.LoadFile(xml_file_path.c_str()))
    {
        tinyxml2::XMLElement* root = doc.FirstChildElement(s_sDS.c_str());
        if (root == nullptr)
        {
            root = doc.FirstChildElement(s_sDS_Snapshots.c_str());
            if (root == nullptr)
            {
                LOG_ERROR("Invalid config or snapshot file");
                return;
            }
            else
            {
                loadSnapshots(xml_file_path);
                validate_ = true;
                auto_shutdown = true;
                LOG_INFO("Loaded snapshot file");
                return;
            }
        }

        // config file, we must validate
        validate_ = true;

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
            if (simples != nullptr)
            {
                tinyxml2::XMLElement* simple = simples->FirstChildElement(s_sSimple.c_str());
                while (simple != nullptr)
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
                    // if we want an output file do not validate
                    validate_ = false;
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
        LOG_ERROR("Config file not found.");
        return;
    }

    correctly_created_ = true;
    LOG_INFO("File " << xml_file_path << " parsed successfully.");
}

DSManager::DSManager(
        const std::set<std::string>& xml_snapshot_files,
        const std::string& output_file)
    : no_callbacks(true)
    , auto_shutdown(true)
    , enable_prefix_validation(true)
    , last_PDP_callback_(Snapshot::_steady_clock)
    , last_EDP_callback_(Snapshot::_steady_clock)
{
    // validating snapshots files
    validate_ = true;
    // keep output_file if any
    snapshots_output_file = output_file;

    for (const std::string& file : xml_snapshot_files)
    {
        if (loadSnapshots(file))
        {
            LOG("Loaded snapshot file " << file);
        }
        else
        {
            // if some file fails, do not validate
            validate_ = false;
        }
    }
}

void DSManager::runEvents(
        std::istream& in /*= std::cin*/,
        std::ostream& out /*= std::cout*/)
{
    // Order the event list
    std::sort(events.begin(), events.end(), [](LateJoinerData* p1, LateJoinerData* p2) -> bool
            {
                return *p1 < *p2;
            });

    // traverse the list
    for (LateJoinerData* p : events)
    {
        // Wait till specified time
        p->Wait();
        // execute
        (*p)(*this);
    }

    // multiple processes sync delay
    if (!snapshots_output_file.empty())
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
        DomainParticipant* s)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    assert(servers[s->guid()] == nullptr);
    servers[s->guid()] = s;
}

void DSManager::addClient(
        DomainParticipant* c)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    assert(clients[c->guid()] == nullptr);
    clients[c->guid()] = c;
}

void DSManager::addSimple(
        DomainParticipant* s)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    assert(simples[s->guid()] == nullptr);
    simples[s->guid()] = s;
}

DomainParticipant* DSManager::getParticipant(
        GUID_t& id)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    created_entity_map::iterator iter = std::find_if(entity_map.begin(), entity_map.end(),
                    [id](const std::pair<GUID_t, ParticipantCreatedEntityInfo> info)
                    {
                        return info.first == id;
                    } );
    if (iter != entity_map.end())
    {
        return iter->second.participant;
    }

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
    else if ((it = simples.find(id)) != simples.end())
    {
        return it->second;
    }

    return nullptr;
}

DomainParticipant* DSManager::removeParticipant(
        GUID_t& id)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    DomainParticipant* ret = nullptr;

    // update database
    bool returnState = state.RemoveParticipant(id);
    if (!returnState)
    {
        LOG_ERROR("Error during database deletion" << id);
    }
    // remove any related pubs-subs
    {
        data_writer_map paux;
        std::remove_copy_if(data_writers.begin(), data_writers.end(), std::inserter(paux, paux.begin()),
                [&id](data_writer_map::value_type it)
                {
                    return id.guidPrefix == it.first.guidPrefix;
                });
        data_writers.swap(paux);

        data_reader_map saux;
        std::remove_copy_if(data_readers.begin(), data_readers.end(), std::inserter(saux, saux.begin()),
                [&id](data_reader_map::value_type it)
                {
                    return id.guidPrefix == it.first.guidPrefix;
                });
        data_readers.swap(saux);
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
    else if ((it = simples.find(id)) != simples.end())
    {
        ret = it->second;
        simples.erase(it);
    }

    return ret;
}

void DSManager::addDataReader(
        DataReader* sub)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    assert(data_readers[sub->guid()] == nullptr);
    data_readers[sub->guid()] = sub;
}

DataReader* DSManager::getDataReader(
        GUID_t& id)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    data_reader_map::iterator it = data_readers.find(id);
    if (it != data_readers.end())
    {
        return it->second;
    }

    return nullptr;
}

DataReader* DSManager::removeSubscriber(
        GUID_t& id)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    DataReader* ret = nullptr;

    data_reader_map::iterator it = data_readers.find(id);
    if (it != data_readers.end())
    {
        ret = it->second;
        data_readers.erase(it);
    }

    return ret;
}

ReturnCode_t DSManager::deleteDataReader(
        DataReader* dr)
{
    fastdds::dds::Subscriber* sub = nullptr;
    {
        std::lock_guard<std::recursive_mutex> lock(management_mutex);
        GUID_t participant_guid = dr->get_subscriber()->get_participant()->guid();
        sub = entity_map[participant_guid].subscriber;
    }
    if (sub != nullptr)
    {
        return sub->delete_datareader(dr);
    }
    return ReturnCode_t::RETCODE_ERROR;
}

ReturnCode_t DSManager::deleteDataWriter(
        DataWriter* dw)
{
    fastdds::dds::Publisher* pub = nullptr;
    {
        std::lock_guard<std::recursive_mutex> lock(management_mutex);
        GUID_t participant_guid = dw->get_publisher()->get_participant()->guid();
        pub = entity_map[participant_guid].publisher;
    }
    if (pub != nullptr)
    {
        return pub->delete_datawriter(dw);
    }
    return ReturnCode_t::RETCODE_ERROR;
}

ReturnCode_t DSManager::deleteParticipant(
        DomainParticipant* participant)
{
    {

        std::lock_guard<std::recursive_mutex> lock(management_mutex);

        participant->set_listener(nullptr);

        entity_map.erase(participant->guid());

        ReturnCode_t ret = participant->delete_contained_entities();
        if (ret != ReturnCode_t::RETCODE_OK)
        {
            LOG_ERROR("Error cleaning up participant entities");   
        }

    }

    return DomainParticipantFactory::get_instance()->delete_participant(participant);

}

template<>
void DSManager::setDomainEntityTopic(
        fastdds::dds::DataWriter* entity,
        Topic* t)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    
    entity_map[entity->get_publisher()->get_participant()->guid()].publisherTopic = t;
}

template<>
void DSManager::setDomainEntityTopic(
        fastdds::dds::DataReader* entity,
        Topic* t)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    entity_map[entity->get_subscriber()->get_participant()->guid()].subscriberTopic = t;
}

template<>
void DSManager::setDomainEntityType(
        fastdds::dds::Publisher* entity,
        TopicDataType* t)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    entity_map[entity->get_participant()->guid()].publisherType = t;
}

template<>
void DSManager::setDomainEntityType(
        fastdds::dds::Subscriber* entity,
        TopicDataType* t)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    entity_map[entity->get_participant()->guid()].subscriberType = t;
}

void DSManager::addDataWriter(
        DataWriter* dw)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    assert(data_writers[dw->guid()] == nullptr);
    data_writers[dw->guid()] = dw;
}

DataWriter* DSManager::getDataWriter(
        GUID_t& id)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    data_writer_map::iterator it = data_writers.find(id);
    if (it != data_writers.end())
    {
        return it->second;
    }

    return nullptr;
}

DataWriter* DSManager::removePublisher(
        GUID_t& id)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    DataWriter* ret = nullptr;

    data_writer_map::iterator it = data_writers.find(id);
    if (it != data_writers.end())
    {
        ret = it->second;
        data_writers.erase(it);
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
    types::DynamicPubSubType*& dynamic_type = loaded_types[type_name];

    if (dynamic_type == nullptr)
    {
        dynamic_type = xmlparser::XMLProfileManager::CreateDynamicPubSubType(type_name);
        if (dynamic_type == nullptr)
        {
            loaded_types.erase(type_name);

            LOG_ERROR("DSManager::setType couldn't create type " << type_name);
            return nullptr;
        }
    }

    return dynamic_type;
}

template<>
void DSManager::getPubSubEntityFromParticipantGuid< eprosima::fastdds::dds::DataWriter >(
        GUID_t& id,
        DomainEntity*& pubsub)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    pubsub = entity_map[id].publisher;
}

template<>
void DSManager::getPubSubEntityFromParticipantGuid< eprosima::fastdds::dds::DataReader >(
        GUID_t& id,
        DomainEntity*& pubsub)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    pubsub = entity_map[id].subscriber;
}

void DSManager::setParticipantInfo(
        GUID_t guid,
        ParticipantCreatedEntityInfo info)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    entity_map[guid] = info;
}

void DSManager::setParticipantTopic(
        DomainParticipant* p,
        Topic* t)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    
    created_entity_map::iterator it = entity_map.find(p->guid());
    if (it != entity_map.end())
    {
        entity_map[p->guid()].registeredTopics[t->get_name()] = t;
    }
}

Topic* DSManager::getParticipantTopicByName(
        DomainParticipant* p,
        std::string name)
{
    Topic* returnTopic = nullptr;
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    created_entity_map::iterator it = entity_map.find(p->guid());
    if (it != entity_map.end())
    {
        returnTopic = entity_map[p->guid()].registeredTopics[name];
    }
    else
    {
        return nullptr;
    }

    return returnTopic;
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
    {
        // make sure all other threads don't modify the state
        std::lock_guard<std::recursive_mutex> lock(management_mutex);

        if (no_callbacks)
        {
            return; // DSManager already terminated
        }

        no_callbacks = true;

    }

    // Since we cannot use delete_contained_enttities for now...

    for (const auto& entity: entity_map )
    {
        entity.second.participant->set_listener(nullptr);

        ReturnCode_t ret = entity.second.participant->delete_contained_entities();
        if (ReturnCode_t::RETCODE_OK != ret)
        {
            LOG_ERROR("Error cleaning up participant entities");
        }

        ret = DomainParticipantFactory::get_instance()->delete_participant(entity.second.participant);
        if (ReturnCode_t::RETCODE_OK != ret)
        {
            LOG_ERROR("Error deleting Participant");
        }
    }

    entity_map.clear();

    data_readers.clear();

    data_writers.clear();

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
}

void DSManager::loadServer(
        tinyxml2::XMLElement* server)
{
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
        {
            // may be empty on purpose (for creating dummie clients)
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
    serverLocator_map::mapped_type& lists = server_locators[guid];
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

        tinyxml2::XMLElement* rserver = server_list->FirstChildElement(s_sRServer.c_str());

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
            serverLocator_map::mapped_type& lists = server_locators[srv.GetParticipant()];
            srv.metatrafficMulticastLocatorList = lists.first;
            srv.metatrafficUnicastLocatorList = lists.second;

            list.push_back(std::move(srv));

            rserver = rserver->NextSiblingElement(s_sRServer.c_str());
        }
    }

    if (shared_memory_off_)
    {
        // Desactivate transport by default
        atts.rtps.useBuiltinTransports = false;

        auto udp_transport = std::make_shared<UDPv4TransportDescriptor>();
        atts.rtps.userTransports.push_back(udp_transport);
    }

    // We define the PDP as external (when moved to fast library it would be SERVER)
    DiscoverySettings& b = atts.rtps.builtin.discovery_config;
    (void)b;
    assert(b.discoveryProtocol == SERVER || b.discoveryProtocol == BACKUP);

    // Create the participant or the associated events
    DelayedParticipantCreation event(creation_time, std::move(atts), &DSManager::addServer);

    if (creation_time == getTime())
    {
        event(*this);
    }
    else
    {
        // late joiner
        events.push_back(new DelayedParticipantCreation(std::move(event)));
    }

    if (removal_time != getTime())
    {
        // early leaver
        events.push_back(new DelayedParticipantDestruction(removal_time, guid));
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
#if FASTRTPS_VERSION_MAJOR >= 2
    if (atts.rtps.builtin.discovery_config.discoveryProtocol != DiscoveryProtocol_t::CLIENT &&
            atts.rtps.builtin.discovery_config.discoveryProtocol != DiscoveryProtocol_t::SUPER_CLIENT)
#else
    if (atts.rtps.builtin.discovery_config.discoveryProtocol != DiscoveryProtocol_t::CLIENT)
#endif // if FASTRTPS_VERSION_MAJOR >= 2
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
        GuidPrefix_t& prefix = srv.guidPrefix;

        if (!(std::istringstream(server) >> prefix) &&
                (prefix == c_GuidPrefix_Unknown))
        {
            LOG_ERROR("server attribute must provide a prefix"); // at least for now
            return;
        }

        RemoteServerList_t& list = atts.rtps.builtin.discovery_config.m_DiscoveryServers;
        list.clear(); // server elements take precedence over profile ones

        // load the locator lists
        serverLocator_map::mapped_type& lists = server_locators[srv.GetParticipant()];
        srv.metatrafficMulticastLocatorList = lists.first;
        srv.metatrafficUnicastLocatorList = lists.second;

        list.push_back(std::move(srv));
    }
    else
    {
        // load the server list (if present) and update the atts.rtps.builtin
        tinyxml2::XMLElement* server_list = client->FirstChildElement(s_sSL.c_str());

        if (server_list != nullptr)
        {
            RemoteServerList_t& list = atts.rtps.builtin.discovery_config.m_DiscoveryServers;
            list.clear(); // server elements take precedence over profile ones

            tinyxml2::XMLElement* rserver = server_list->FirstChildElement(s_sRServer.c_str());

            while (rserver != nullptr)
            {
                RemoteServerList_t::value_type srv;
                GuidPrefix_t& prefix = srv.guidPrefix;

                // load the prefix
                const char* cprefix = rserver->Attribute(DSxmlparser::PREFIX);

                if (cprefix != nullptr && !(std::istringstream(cprefix) >> prefix)
                        && (prefix == c_GuidPrefix_Unknown))
                {
                    LOG_ERROR("RServers must provide a prefix"); // at least for now
                    return;
                }

                // load the locator lists
                serverLocator_map::mapped_type& lists = server_locators[srv.GetParticipant()];
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
        TCPTransportDescriptor* pT = nullptr;
        std::shared_ptr<TCPv4TransportDescriptor> p4;
        std::shared_ptr<TCPv6TransportDescriptor> p6;

        // Supossed to be only one transport of each class
        for (auto sp : atts.rtps.userTransports)
        {
            pT = dynamic_cast<TCPTransportDescriptor*>(sp.get());

            if (pT != nullptr)
            {
                if (!p4)
                {
                    // try to find a descriptor matching the listener port setup
                    if (p4 = std::dynamic_pointer_cast<TCPv4TransportDescriptor>(sp))
                    {
                        continue;
                    }
                }

                if (!p6)
                {
                    // try to find a descriptor matching the listener port setup
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
            for (std::shared_ptr<TransportDescriptorInterface>& sp : atts.rtps.userTransports)
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
            for (std::shared_ptr<TransportDescriptorInterface>& sp : atts.rtps.userTransports)
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

    if (shared_memory_off_)
    {
        // Desactivate transport by default
        atts.rtps.useBuiltinTransports = false;

        auto udp_transport = std::make_shared<UDPv4TransportDescriptor>();
        atts.rtps.userTransports.push_back(udp_transport);
    }

    GUID_t guid(atts.rtps.prefix, c_EntityId_RTPSParticipant);
    DelayedParticipantDestruction* pD = nullptr;
    DelayedParticipantCreation* pC = nullptr;

    if (removal_time != getTime())
    {
        // early leaver
        pD = new DelayedParticipantDestruction(removal_time, guid);
        events.push_back(pD);
    }

    // Create the participant or the associated events
    DelayedParticipantCreation event(creation_time, std::move(atts), &DSManager::addClient, pD);

    if (creation_time == getTime())
    {
        event(*this);

        // get the client guid
        guid = event.participant_guid;
    }
    else
    {
        // late joiner
        pC = new DelayedParticipantCreation(std::move(event));
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

void DSManager::loadSimple(
        tinyxml2::XMLElement* simple)
{
    // check if we need to create an event
    std::chrono::steady_clock::time_point creation_time, removal_time;
    creation_time = removal_time = getTime();

    {
        const char* creation_time_str = simple->Attribute(s_sCreationTime.c_str());
        if (creation_time_str != nullptr)
        {
            int aux;
            std::istringstream(creation_time_str) >> aux;
            creation_time += std::chrono::seconds(aux);
        }

        const char* removal_time_str = simple->Attribute(s_sRemovalTime.c_str());
        if (removal_time_str != nullptr)
        {
            int aux;
            std::istringstream(removal_time_str) >> aux;
            removal_time += std::chrono::seconds(aux);
        }
    }

    // simple are created for debugging purposes
    // profile name is not mandatory
    const char* profile_name = simple->Attribute(DSxmlparser::PROFILE_NAME);

    ParticipantAttributes atts;

    if (profile_name != nullptr)
    {
        // retrieve profile attributes
        if (xmlparser::XMLP_ret::XML_OK !=
                xmlparser::XMLProfileManager::fillParticipantAttributes(std::string(profile_name), atts))
        {
            LOG_ERROR("DSManager::loadSimple couldn't load profile " << profile_name);
            return;
        }

        // we must assert that DiscoveryProtocol is CLIENT
        if (atts.rtps.builtin.discovery_config.discoveryProtocol != DiscoveryProtocol_t::SIMPLE)
        {
            LOG_ERROR(
                "DSManager::loadSimple try to create a simple participant with an incompatible profile: " <<
                    profile_name);
            return;
        }

    }

    // pick the client's name (isn't mandatory to provide it). Takes precedence over profile provided.
    const char* name = simple->Attribute(DSxmlparser::NAME);
    if (name != nullptr)
    {
        atts.rtps.setName(name);
    }

    GUID_t guid(atts.rtps.prefix, c_EntityId_RTPSParticipant);
    DelayedParticipantDestruction* pD = nullptr;
    DelayedParticipantCreation* pC = nullptr;

    if (removal_time != getTime())
    {
        // early leaver
        pD = new DelayedParticipantDestruction(removal_time, guid);
        events.push_back(pD);
    }

    // Create the participant or the associated events
    DelayedParticipantCreation event(creation_time, std::move(atts), &DSManager::addSimple, pD);

    if (creation_time == getTime())
    {
        event(*this);

        // get the client guid
        guid = event.participant_guid;
    }
    else
    {
        // late joiner
        pC = new DelayedParticipantCreation(std::move(event));
        events.push_back(pC);
    }

    // Once the participant is created we create the associated endpoints
    tinyxml2::XMLElement* pub = simple->FirstChildElement(DSxmlparser::PUBLISHER);
    while (pub != nullptr)
    {
        loadPublisher(guid, pub, pC);
        pub = pub->NextSiblingElement(DSxmlparser::PUBLISHER);
    }

    tinyxml2::XMLElement* sub = simple->FirstChildElement(DSxmlparser::SUBSCRIBER);
    while (sub != nullptr)
    {
        loadSubscriber(guid, sub, pC);
        sub = sub->NextSiblingElement(DSxmlparser::SUBSCRIBER);
    }
}

void DSManager::loadSubscriber(
        GUID_t& part_guid,
        tinyxml2::XMLElement* sub,
        DelayedParticipantCreation* pPC /*= nullptr*/,
        DelayedParticipantDestruction* pPD /*= nullptr*/)
{
    assert(sub != nullptr);

    // check if we need to create an event
    std::chrono::steady_clock::time_point creation_time, removal_time;

    // Match the creation and destruction times to the participant
    if (nullptr != pPC)
    {
        creation_time = pPC->executionTime();
        // prevent creation before the participant
        creation_time += std::chrono::nanoseconds(1);
    }
    else
    {
        creation_time = getTime();
    }

    if (nullptr != pPD)
    {
        removal_time = pPC->executionTime();
        // prevent destruction after the participant
        creation_time -= std::chrono::nanoseconds(1);
    }
    else
    {
        removal_time = getTime();
    }

    {
        const char* creation_time_str = sub->Attribute(s_sCreationTime.c_str());
        if (creation_time_str != nullptr)
        {
            int aux;
            std::istringstream(creation_time_str) >> aux;
            creation_time = getTime() + std::chrono::seconds(aux);
        }

        const char* removal_time_str = sub->Attribute(s_sRemovalTime.c_str());
        if (removal_time_str != nullptr)
        {
            int aux;
            std::istringstream(removal_time_str) >> aux;
            removal_time = getTime() + std::chrono::seconds(aux);
        }
    }

    // data_readers are created for debugging purposes
    // default topic is the static HelloWorld one
    const char* profile_name = sub->Attribute(DSxmlparser::PROFILE_NAME);

    SubscriberAttributes* subatts = new SubscriberAttributes();

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
            LOG_ERROR("DSManager::loadSubscriber couldn't load topic profile ");
            return;
        }
    }

    DelayedEndpointDestruction<DataReader>* pDE = nullptr; // subscriber destruction event

    if (removal_time != getTime())
    {
        // Destruction event needs the endpoint guid
        // that would be provided in creation
        pDE = new DelayedEndpointDestruction<DataReader>(removal_time);
        events.push_back(pDE);
    }

    DelayedEndpointCreation<DataReader> event(creation_time, subatts, part_guid, pDE, pPC);

    if (creation_time == getTime())
    {
        event(*this);
    }
    else
    {
        // late joiner
        events.push_back(new DelayedEndpointCreation<DataReader>(std::move(event)));
    }
}

void DSManager::loadPublisher(
        GUID_t& part_guid,
        tinyxml2::XMLElement* sub,
        DelayedParticipantCreation* pPC /*= nullptr*/,
        DelayedParticipantDestruction* pPD /*= nullptr*/)
{
    assert(sub != nullptr);

    // check if we need to create an event
    std::chrono::steady_clock::time_point creation_time, removal_time;

    // Match the creation and destruction times to the participant
    if ( nullptr != pPC)
    {
        creation_time = pPC->executionTime();
        // prevent creation before the participant
        creation_time += std::chrono::nanoseconds(1);
    }
    else
    {
        creation_time = getTime();
    }

    if (nullptr != pPD)
    {
        removal_time = pPC->executionTime();
        // prevent destruction after the participant
        creation_time -= std::chrono::nanoseconds(1);
    }
    else
    {
        removal_time = getTime();
    }

    {
        const char* creation_time_str = sub->Attribute(s_sCreationTime.c_str());
        if (creation_time_str != nullptr)
        {
            int aux;
            std::istringstream(creation_time_str) >> aux;
            creation_time = getTime() + std::chrono::seconds(aux);
        }

        const char* removal_time_str = sub->Attribute(s_sRemovalTime.c_str());
        if (removal_time_str != nullptr)
        {
            int aux;
            std::istringstream(removal_time_str) >> aux;
            removal_time = getTime() + std::chrono::seconds(aux);
        }
    }

    // data_readers are created for debugging purposes
    // default topic is the static HelloWorld one
    const char* profile_name = sub->Attribute(DSxmlparser::PROFILE_NAME);

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

    DelayedEndpointDestruction<DataWriter>* pDE = nullptr; // subscriber destruction event

    if (removal_time != getTime())
    {
        // Destruction event needs the endpoint guid
        // that would be provided in creation
        pDE = new DelayedEndpointDestruction<DataWriter>(removal_time);
        events.push_back(pDE);
    }

    DelayedEndpointCreation<DataWriter> event(creation_time, pubatts, part_guid, pDE, pPC);

    if (creation_time == getTime())
    {
        event(*this);
    }
    else
    {
        // late joiner
        events.push_back(new DelayedEndpointCreation<DataWriter>(std::move(event)));
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
    const char* time_str = snapshot->Attribute(s_sTime.c_str());

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

    // show subscriber liveliness info
    bool show_liveliness = snapshot->BoolAttribute(s_sShowLiveliness.c_str(), false);

    // Get the description from the tag
    std::string description(snapshot->GetText());

    // Add the event
    events.push_back(new DelayedSnapshot(time, description, someone, show_liveliness));
}

void DSManager::MapServerInfo(
        tinyxml2::XMLElement* server)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    uint8_t ident = 1;

    // profile name is mandatory
    std::string profile_name(server->Attribute(DSxmlparser::PROFILE_NAME));

    if (profile_name.empty())
    {
        // its doesn't log as error because may be empty on purpose
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
        tinyxml2::XMLElement* list = LP->FirstChildElement(DSxmlparser::META_MULTI_LOC_LIST);

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

void DSManager::on_participant_discovery(
        DomainParticipant* participant,
        ParticipantDiscoveryInfo&& info)
{
    bool server = false;
    const GUID_t& partid = info.info.m_guid;

    // if the callback origin was removed ignore
    GUID_t srcGuid = participant->guid();
    if ( nullptr == getParticipant(srcGuid))
    {
        LOG_INFO("Received onParticipantDiscovery callback from unknown participant: " << srcGuid);
        return;
    }

    LOG_INFO("Participant " << participant->get_qos().name().to_string() << " reports a participant "
                            << info.info.m_participantName << " is " << info.status << ". Prefix " << partid);

    std::chrono::steady_clock::time_point callback_time = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::recursive_mutex> lock(management_mutex);

        // update last_callback time
        last_PDP_callback_ = callback_time;

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
            state.AddParticipant(srcGuid, partid, info.info.m_participantName.to_string(), callback_time,
                    server);
            break;
        }
        case ParticipantDiscoveryInfo::REMOVED_PARTICIPANT:
        case ParticipantDiscoveryInfo::DROPPED_PARTICIPANT:
        {
            state.RemoveParticipant(srcGuid, partid);
            break;
        }
        default:
            break;
    }
}

void DSManager::on_subscriber_discovery(
        DomainParticipant* participant,
        ReaderDiscoveryInfo&& info)
{
    typedef ReaderDiscoveryInfo::DISCOVERY_STATUS DS;

    // if the callback origin was removed ignore
    GUID_t srcGuid = participant->guid();
    if ( nullptr == getParticipant(srcGuid))
    {
        LOG_INFO("Received SubscriberDiscovery callback from unknown participant: " << srcGuid);
        return;
    }

    const GUID_t& subsid = info.info.guid();
    GUID_t partid = iHandle2GUID(info.info.RTPSParticipantKey());

    // non reported info
    std::string part_name;

    std::chrono::steady_clock::time_point callback_time = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::recursive_mutex> lock(management_mutex);

        // update last_callback time
        last_EDP_callback_ = callback_time;

        participant_map::iterator it;

        if (!no_callbacks)
        {
            // is one of ours?
            if ((it = servers.find(partid)) != servers.end() ||
                    (it = clients.find(partid)) != clients.end() ||
                    (it = simples.find(partid)) != simples.end())
            {
                part_name = it->second->get_qos().name().to_string();
            }
        }
        else
        {
            // stick to non-DSManager info
            for (const ParticipantDiscoveryItem* p : state.FindParticipant(partid))
            {
                if (!p->participant_name.empty())
                {
                    part_name = p->participant_name;
                }
            }
        }
    }

    if (part_name.empty())
    {
        // if remote use prefix instead of name
        part_name = static_cast<std::ostringstream&>(std::ostringstream() << partid).str();
    }

    switch (info.status)
    {
        case DS::DISCOVERED_READER:
            state.AddDataReader(srcGuid, partid, subsid, info.info.typeName().to_string(),
                    info.info.topicName().to_string(), callback_time);
            break;
        case DS::REMOVED_READER:
            state.RemoveDataReader(srcGuid, partid, subsid);
            break;
        default:
            break;
    }

    LOG_INFO("Participant " << participant->get_qos().name().to_string() << " reports a subscriber of participant "
                            << part_name << " is " << info.status << " with typename: " << info.info.typeName()
                            << " topic: " << info.info.topicName() << " GUID: " << subsid);
}

void DSManager::on_publisher_discovery(
        DomainParticipant* participant,
        WriterDiscoveryInfo&& info)
{
    typedef WriterDiscoveryInfo::DISCOVERY_STATUS DS;

    // if the callback origin was removed ignore
    GUID_t srcGuid = participant->guid();
    if ( nullptr == getParticipant(srcGuid))
    {
        LOG_INFO("Received PublisherDiscovery callback from unknown participant: " << srcGuid);
        return;
    }

    const GUID_t& pubsid = info.info.guid();
    GUID_t partid = iHandle2GUID(info.info.RTPSParticipantKey());

    // non reported info
    std::string part_name;

    std::chrono::steady_clock::time_point callback_time = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::recursive_mutex> lock(management_mutex);

        // update last_callback time
        last_EDP_callback_ = callback_time;

        if (!no_callbacks)
        {
            // is one of ours?
            participant_map::iterator it;

            if ((it = servers.find(partid)) != servers.end() ||
                    (it = clients.find(partid)) != clients.end() ||
                    (it = simples.find(partid)) != simples.end())
            {
                part_name = it->second->get_qos().name().to_string();
            }
        }
        else
        {
            // stick to non-DSManager info
            for (const ParticipantDiscoveryItem* p : state.FindParticipant(partid))
            {
                if (!p->participant_name.empty())
                {
                    part_name = p->participant_name;
                }
            }
        }
    }

    if (part_name.empty())
    {
        // if remote use prefix instead of name
        part_name = static_cast<std::ostringstream&>(std::ostringstream() << partid).str();
    }

    switch (info.status)
    {
        case DS::DISCOVERED_WRITER:

            state.AddDataWriter(srcGuid,
                    partid,
                    pubsid,
                    info.info.typeName().to_string(),
                    info.info.topicName().to_string(),
                    callback_time);
            break;
        case DS::REMOVED_WRITER:

            state.RemoveDataWriter(srcGuid, partid, pubsid);
            break;
        default:
            break;
    }

    LOG_INFO("Participant " << participant->get_qos().name().to_string() << " reports a publisher of participant "
                            << part_name << " is " << info.status << " with typename: " << info.info.typeName()
                            << " topic: " << info.info.topicName() << " GUID: " << pubsid);
}

void DSManager::on_liveliness_changed(
        DataReader* sub,
        const LivelinessChangedStatus& status)
{
    state.UpdateSubLiveliness(sub->guid(), status.alive_count, status.not_alive_count);
}

std::ostream& eprosima::discovery_server::operator <<(
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
        default: // unknown value, error
            o.setstate(std::ios::failbit);
    }

    return o;
}

std::ostream& eprosima::discovery_server::operator <<(
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
        default: // unknown value, error
            o.setstate(std::ios::failbit);
    }

    return o;
}

std::ostream& eprosima::discovery_server::operator <<(
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
        default: // unknown value, error
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
        const std::string& desc /* = std::string()*/,
        bool someone,
        bool show_liveliness)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    snapshots.push_back(state.GetState());

    Snapshot& shot = snapshots.back();
    shot._time = tp;
    shot.last_PDP_callback_ = last_PDP_callback_;
    shot.last_EDP_callback_ = last_EDP_callback_;
    shot._des = desc;
    shot.if_someone = someone;
    shot.show_liveliness_ = show_liveliness;

    // Add any simple, client or server isolated information
    // those have not make any callbacks if no subscriber or publisher

    participant_map temp(servers);
    temp.insert(clients.begin(), clients.end());
    temp.insert(simples.begin(), simples.end());

    std::function<bool(const participant_map::value_type&, const Snapshot::value_type&)> pred(
        [](const participant_map::value_type& p1, const Snapshot::value_type& p2)
        {
            return p1.first == p2.endpoint_guid;
        }
        );

    std::pair<participant_map::const_iterator, Snapshot::const_iterator> res =
            std::mismatch(temp.cbegin(), temp.cend(), shot.cbegin(), shot.cend(), pred);

    while (res.first != temp.end())
    {
        // res.first participant hasn't any discovery info in this Snapshot
        res.second = shot.emplace_hint(res.second, ParticipantDiscoveryDatabase(res.first->first));
        res = std::mismatch(res.first, temp.cend(), res.second, shot.cend(), pred);
    }

    return shot;
}

/*static*/
bool DSManager::allKnowEachOther(
        const Snapshot& shot)
{
    // nobody discovered is bad?
    if (shot.if_someone && (shot.empty() || shot.begin()->empty()))
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

std::string DSManager::successMessage()
{
    if (snapshots.empty())
    {
        // direct run
        return "Discovery Server run succeeded!";
    }

    return "Output file validation succeeded!";
}

bool DSManager::loadSnapshots(
        const std::string& file)
{
    using namespace tinyxml2;
    XMLDocument xmlDoc;

    if (tinyxml2::XML_SUCCESS != xmlDoc.LoadFile(file.c_str()))
    {
        LOG_ERROR("Couldn't parse the file: " << file);
        return false;
    }

    XMLNode* pRoot = xmlDoc.FirstChildElement(s_sDS_Snapshots.c_str());

    if (nullptr == pRoot)
    {
        LOG_ERROR("Not a valid Snapshot file wrong root element: " << file);
        return false;
    }

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

    // add default comment
    xmlDoc.InsertFirstChild(xmlDoc.NewDeclaration(nullptr));

    XMLElement* pRoot = xmlDoc.NewElement(s_sDS_Snapshots.c_str());
    // add the specific schema
    pRoot->SetAttribute("xmlns", "http://www.eprosima.com/XMLSchemas/ds-snapshot");

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
