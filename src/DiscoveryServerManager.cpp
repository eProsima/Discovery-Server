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
#include <fstream>

#include <tinyxml2.h>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/rtps/RTPSDomain.hpp>
#include <fastdds/rtps/transport/UDPv4TransportDescriptor.hpp>
#include <fastdds/rtps/transport/TCPv4TransportDescriptor.hpp>
#include <fastdds/rtps/transport/TCPv6TransportDescriptor.hpp>

#include "DiscoveryServerManager.h"
#include "IDs.h"
#include "LateJoiner.h"
#include "log/DSLog.h"

using namespace eprosima::fastdds;
using namespace eprosima::discovery_server;

// non exported from Fast DDS (watch out they may be updated)
namespace eprosima {
namespace fastdds {
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
} // namespace fastdds
} // namespace eprosima

/*static members*/
TopicAttributes DiscoveryServerManager::builtin_defaultTopic("HelloWorldTopic", "HelloWorld");
const std::regex DiscoveryServerManager::ipv4_regular_expression("^((?:[0-9]{1,3}\\.){3}[0-9]{1,3})?:?(?:(\\d+))?$");
const std::chrono::seconds DiscoveryServerManager::last_snapshot_delay_ = std::chrono::seconds(1);

DiscoveryServerManager::DiscoveryServerManager(
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
            // first make loadProfiles parse the config file. Afterwards loaded info is accessible
            // through get_participant_qos_from_profile() and related

            tinyxml2::XMLElement* profiles = child->FirstChildElement(DSxmlparser::PROFILES);
            if (profiles != nullptr)
            {
                tinyxml2::XMLPrinter printer;
                profiles->Accept(&printer);
                std::string xmlString = R"(")" + std::string(printer.CStr()) + R"(")";
                if (RETCODE_OK ==
                        DomainParticipantFactory::get_instance()->load_XML_profiles_string(xmlString.c_str(),
                        std::string(printer.CStr()).length()))
                {
                    LOG_INFO("Profiles parsed successfully.");
                }
                else
                {
                    LOG_ERROR("Error parsing profiles!");
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

            tinyxml2::XMLElement* environment = child->FirstChildElement(s_sEnvironment.c_str());
            if (environment)
            {
                char* data;
                data = getenv("FASTDDS_ENVIRONMENT_FILE");
                if (nullptr != data)
                {
                    std::ofstream output_file(data);
                    output_file << "{" << std::endl;
                    output_file << '\t' << std::endl;
                    output_file << "}" << std::endl;
                }
                else
                {
                    LOG_ERROR("Empty FASTDDS_ENVIRONMENT_FILE variable");
                    return;
                }

                tinyxml2::XMLElement* change = environment->FirstChildElement(s_sChange.c_str());
                while (change != nullptr)
                {
                    loadEnvironmentChange(change);
                    change = change->NextSiblingElement(s_sChange.c_str());
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

void DiscoveryServerManager::runEvents(
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

void DiscoveryServerManager::addServer(
        DomainParticipant* s)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    assert(servers[s->guid()] == nullptr);
    servers[s->guid()] = s;
}

void DiscoveryServerManager::addClient(
        DomainParticipant* c)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    assert(clients[c->guid()] == nullptr);
    clients[c->guid()] = c;
}

void DiscoveryServerManager::addSimple(
        DomainParticipant* s)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    assert(simples[s->guid()] == nullptr);
    simples[s->guid()] = s;
}

DomainParticipant* DiscoveryServerManager::getParticipant(
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

DomainParticipant* DiscoveryServerManager::removeParticipant(
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
    // remove any related datareaders/writers
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

void DiscoveryServerManager::addDataReader(
        DataReader* dr)
{
    if (dr == nullptr)
    {
        LOG_ERROR("Error adding DataReader. Null pointer");
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    assert(data_readers[dr->guid()] == nullptr);
    data_readers[dr->guid()] = dr;
}

DataReader* DiscoveryServerManager::getDataReader(
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

DataReader* DiscoveryServerManager::removeSubscriber(
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

ReturnCode_t DiscoveryServerManager::deleteDataReader(
        DataReader* dr)
{
    if (dr == nullptr)
    {
        return RETCODE_ERROR;
    }
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
    return RETCODE_ERROR;
}

ReturnCode_t DiscoveryServerManager::deleteDataWriter(
        DataWriter* dw)
{
    if (dw == nullptr)
    {
        return RETCODE_ERROR;
    }
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
    return RETCODE_ERROR;
}

ReturnCode_t DiscoveryServerManager::deleteParticipant(
        DomainParticipant* participant)
{
    {
        if (participant == nullptr)
        {
            return RETCODE_ERROR;
        }

        std::lock_guard<std::recursive_mutex> lock(management_mutex);
        participant->set_listener(nullptr);

        entity_map.erase(participant->guid());

        ReturnCode_t ret = participant->delete_contained_entities();
        if (ret != RETCODE_OK)
        {
            LOG_ERROR("Error cleaning up participant entities");
        }

    }

    return DomainParticipantFactory::get_instance()->delete_participant(participant);

}

template<>
void DiscoveryServerManager::setDomainEntityTopic(
        fastdds::dds::DataWriter* entity,
        Topic* t)
{
    if (entity == nullptr)
    {
        LOG_ERROR("Error setting Domain Entity Topic. Null DataWriter");
        return;
    }
    if (t == nullptr)
    {
        LOG_ERROR("Error setting Domain Entity Topic. Null DataWriter Topic");
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    entity_map[entity->get_publisher()->get_participant()->guid()].publisherTopic = t;
}

template<>
void DiscoveryServerManager::setDomainEntityTopic(
        fastdds::dds::DataReader* entity,
        Topic* t)
{
    if (entity == nullptr)
    {
        LOG_ERROR("Error setting Domain Entity Topic. Null DataReader");
        return;
    }
    if (t == nullptr)
    {
        LOG_ERROR("Error setting Domain Entity Topic. Null DataReader Topic");
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    entity_map[entity->get_subscriber()->get_participant()->guid()].subscriberTopic = t;
}

template<>
void DiscoveryServerManager::setDomainEntityType(
        fastdds::dds::Publisher* entity,
        TopicDataType* t)
{
    if (entity == nullptr)
    {
        LOG_ERROR("Error setting Domain Entity Topic. Null Publisher");
        return;
    }
    if (t == nullptr)
    {
        LOG_ERROR("Error setting Domain Entity Topic. Null Publisher Topic Datatype");
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    entity_map[entity->get_participant()->guid()].publisherType = t;
}

template<>
void DiscoveryServerManager::setDomainEntityType(
        fastdds::dds::Subscriber* entity,
        TopicDataType* t)
{
    if (entity == nullptr)
    {
        LOG_ERROR("Error setting Domain Entity Topic. Null Subscriber");
        return;
    }
    if (t == nullptr)
    {
        LOG_ERROR("Error setting Domain Entity Topic. Null Subscriber Topic Datatype");
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    entity_map[entity->get_participant()->guid()].subscriberType = t;
}

void DiscoveryServerManager::addDataWriter(
        DataWriter* dw)
{
    if (dw == nullptr)
    {
        LOG_ERROR("Error adding DataWriter. Null pointer");
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    assert(data_writers[dw->guid()] == nullptr);
    data_writers[dw->guid()] = dw;
}

DataWriter* DiscoveryServerManager::getDataWriter(
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

DataWriter* DiscoveryServerManager::removePublisher(
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

template<>
void DiscoveryServerManager::getPubSubEntityFromParticipantGuid< eprosima::fastdds::dds::DataWriter >(
        GUID_t& id,
        DomainEntity*& pubsub)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    pubsub = entity_map[id].publisher;
}

template<>
void DiscoveryServerManager::getPubSubEntityFromParticipantGuid< eprosima::fastdds::dds::DataReader >(
        GUID_t& id,
        DomainEntity*& pubsub)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    pubsub = entity_map[id].subscriber;
}

void DiscoveryServerManager::setParticipantInfo(
        GUID_t& guid,
        ParticipantCreatedEntityInfo& info)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);
    entity_map[guid] = info;
}

void DiscoveryServerManager::setParticipantTopic(
        DomainParticipant* p,
        Topic* t)
{
    if (p == nullptr)
    {
        LOG_ERROR("Error setting Participant Topic. Null Participant");
        return;
    }
    if (t == nullptr)
    {
        LOG_ERROR("Error setting Participant Topic. Null Topic");
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    created_entity_map::iterator it = entity_map.find(p->guid());
    if (it != entity_map.end())
    {
        entity_map[p->guid()].registeredTopics[t->get_name()] = t;
    }
}

Topic* DiscoveryServerManager::getParticipantTopicByName(
        DomainParticipant* p,
        const std::string& name)
{
    if (p == nullptr)
    {
        LOG_ERROR("Error getting Participant Topic. Null Participant");
        return nullptr;
    }
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

void DiscoveryServerManager::onTerminate()
{
    {
        // make sure all other threads don't modify the state
        std::lock_guard<std::recursive_mutex> lock(management_mutex);

        if (no_callbacks)
        {
            return; // DiscoveryServerManager already terminated
        }

        no_callbacks = true;

    }

    // IMPORTANT: Clear first all clients before cleaning servers
    // Remove all clients
    for (const auto& entity: clients)
    {
        entity.second->set_listener(nullptr);

        ReturnCode_t ret = entity.second->delete_contained_entities();
        if (RETCODE_OK != ret)
        {
            LOG_ERROR("Error cleaning up client entities");
        }

        ret = DomainParticipantFactory::get_instance()->delete_participant(entity.second);
        if (RETCODE_OK != ret)
        {
            LOG_ERROR("Error deleting Client");
        }
    }

    // Remove all simple participants
    // Simple participants could become Clients, so they should be deleted before Servers
    for (const auto& entity: simples)
    {
        entity.second->set_listener(nullptr);

        ReturnCode_t ret = entity.second->delete_contained_entities();
        if (RETCODE_OK != ret)
        {
            LOG_ERROR("Error cleaning up simple entities");
        }

        ret = DomainParticipantFactory::get_instance()->delete_participant(entity.second);
        if (RETCODE_OK != ret)
        {
            LOG_ERROR("Error deleting Simple Discovery Entity");
        }
    }

    // Remove all servers
    for (const auto& entity: servers)
    {
        entity.second->set_listener(nullptr);

        ReturnCode_t ret = entity.second->delete_contained_entities();
        if (RETCODE_OK != ret)
        {
            LOG_ERROR("Error cleaning up server entities");
        }

        ret = DomainParticipantFactory::get_instance()->delete_participant(entity.second);
        if (RETCODE_OK != ret)
        {
            LOG_ERROR("Error deleting Server");
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

DiscoveryServerManager::~DiscoveryServerManager()
{
    if (!snapshots_output_file.empty())
    {
        saveSnapshots(snapshots_output_file);
    }
}

void DiscoveryServerManager::loadServer(
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
    DomainParticipantQos dpQOS;
    if (RETCODE_OK !=
            DomainParticipantFactory::get_instance()->get_participant_qos_from_profile(std::string(profile_name),
            dpQOS))
    {
        LOG_ERROR("DiscoveryServerManager::loadServer couldn't load profile " << profile_name);
        return;
    }

    // server name is either pass as an attribute (preferred to allow profile reuse) or inside the profile
    const char* name = server->Attribute(DSxmlparser::NAME);
    if (name != nullptr)
    {
        dpQOS.name() = name;
    }

    // server GuidPrefix is either pass as an attribute (preferred to allow profile reuse)
    // or inside the profile.
    GuidPrefix_t& prefix = dpQOS.wire_protocol().prefix;
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
        LOG_ERROR("DiscoveryServerManager detected two servers sharing the same prefix " << prefix);
        return;
    }

    // replace the DomainParticipantQOS builtin lists with the ones from server_locators (if present)
    // note that a previous call to DiscoveryServerManager::MapServerInfo
    serverLocator_map::mapped_type& lists = server_locators[guid];
    if (!lists.first.empty() ||
            !lists.second.empty())
    {
        // server elements take precedence over profile ones
        // I copy them because other servers may need this values
        dpQOS.wire_protocol().builtin.metatrafficMulticastLocatorList = lists.first;
        dpQOS.wire_protocol().builtin.metatrafficUnicastLocatorList = lists.second;
    }

    if (shared_memory_off_)
    {
        // Desactivate transport by default
        dpQOS.transport().use_builtin_transports = false;

        auto udp_transport = std::make_shared<UDPv4TransportDescriptor>();
        dpQOS.transport().user_transports.push_back(udp_transport);
    }

    // We define the PDP as external (when moved to fast library it would be SERVER)
    DiscoverySettings& b = dpQOS.wire_protocol().builtin.discovery_config;
    (void)b;
    assert(b.discoveryProtocol == eprosima::fastdds::rtps::DiscoveryProtocol::SERVER || b.discoveryProtocol == eprosima::fastdds::rtps::DiscoveryProtocol::BACKUP);

    // Create the participant or the associated events
    DelayedParticipantCreation event(creation_time, std::move(dpQOS), &DiscoveryServerManager::addServer);
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

void DiscoveryServerManager::loadClient(
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
    DomainParticipantQos dpQOS;
    if (RETCODE_OK !=
            DomainParticipantFactory::get_instance()->get_participant_qos_from_profile(std::string(profile_name),
            dpQOS))
    {
        LOG_ERROR("DiscoveryServerManager::loadClient couldn't load profile " << profile_name);
        return;
    }

    // we must assert that DiscoveryProtocol is CLIENT

    if (dpQOS.wire_protocol().builtin.discovery_config.discoveryProtocol != DiscoveryProtocol::CLIENT &&
            dpQOS.wire_protocol().builtin.discovery_config.discoveryProtocol != DiscoveryProtocol::SUPER_CLIENT)
    {
        LOG_ERROR(
            "DiscoveryServerManager::loadClient try to create a client with an incompatible profile: " <<
                profile_name);
        return;
    }

    // pick the client's name (isn't mandatory to provide it). Takes precedence over profile provided.
    const char* name = client->Attribute(DSxmlparser::NAME);
    if (name != nullptr)
    {
        dpQOS.name() = name;
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
            LOG_ERROR("invalid listening port for server (" << dpQOS.name() << ")"); // at least for now
            return;
        }

        // Look for a suitable user transport
        TCPTransportDescriptor* pT = nullptr;
        std::shared_ptr<TCPv4TransportDescriptor> p4;
        std::shared_ptr<TCPv6TransportDescriptor> p6;

        // Supossed to be only one transport of each class
        for (auto sp : dpQOS.transport().user_transports)
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
            dpQOS.transport().user_transports.push_back(p4);
        }
        else if (p4)
        {
            // create a copy of a tcp4 and replace in transports
            for (std::shared_ptr<TransportDescriptorInterface>& sp : dpQOS.transport().user_transports)
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
            for (std::shared_ptr<TransportDescriptorInterface>& sp : dpQOS.transport().user_transports)
            {
                if (p6 == sp)
                {
                    p6 = std::make_shared<TCPv6TransportDescriptor>(*p6);
                    pT = p6.get();
                    sp = std::dynamic_pointer_cast<TransportDescriptorInterface>(p6);
                }
            }
        }

        if (pT->listening_ports.empty())
        {
            // if no ports are specified, we will use the default ones
            pT->add_listener_port(static_cast<uint16_t>(listening_port));
        }

        // set up WAN if specified
        if (!address.empty() && p4)
        {
            p4->set_WAN_address(address);
        }

    }

    if (shared_memory_off_)
    {
        // Desactivate transport by default
        dpQOS.transport().use_builtin_transports = false;

        auto udp_transport = std::make_shared<UDPv4TransportDescriptor>();
        dpQOS.transport().user_transports.push_back(udp_transport);
    }

    GUID_t guid(dpQOS.wire_protocol().prefix, c_EntityId_RTPSParticipant);
    DelayedParticipantDestruction* destruction_event = nullptr;
    DelayedParticipantCreation* creation_event = nullptr;

    if (removal_time != getTime())
    {
        // early leaver
        destruction_event = new DelayedParticipantDestruction(removal_time, guid);
        events.push_back(destruction_event);
    }

    // Create the participant or the associated events
    DelayedParticipantCreation event(creation_time, std::move(dpQOS), &DiscoveryServerManager::addClient,
            destruction_event);

    if (creation_time == getTime())
    {
        event(*this);

        // get the client guid
        guid = event.participant_guid;
    }
    else
    {
        // late joiner
        creation_event = new DelayedParticipantCreation(std::move(event));
        events.push_back(creation_event);
    }

    // Once the participant is created we create the associated endpoints
    tinyxml2::XMLElement* pub = client->FirstChildElement(DSxmlparser::PUBLISHER);
    while (pub != nullptr)
    {
        loadPublisher(guid, pub, creation_event);
        pub = pub->NextSiblingElement(DSxmlparser::PUBLISHER);
    }

    tinyxml2::XMLElement* sub = client->FirstChildElement(DSxmlparser::SUBSCRIBER);
    while (sub != nullptr)
    {
        loadSubscriber(guid, sub, creation_event);
        sub = sub->NextSiblingElement(DSxmlparser::SUBSCRIBER);
    }
}

void DiscoveryServerManager::loadSimple(
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

    DomainParticipantQos dpQOS;

    if (profile_name != nullptr)
    {
        // retrieve profile attributes
        if (RETCODE_OK !=
                DomainParticipantFactory::get_instance()->get_participant_qos_from_profile(std::string(profile_name),
                dpQOS))
        {
            LOG_ERROR("DiscoveryServerManager::loadSimple couldn't load profile " << profile_name);
            return;
        }

        // we must assert that DiscoveryProtocol is CLIENT
        if (dpQOS.wire_protocol().builtin.discovery_config.discoveryProtocol != DiscoveryProtocol::SIMPLE)
        {
            LOG_ERROR(
                "DiscoveryServerManager::loadSimple try to create a simple participant with an incompatible profile: " <<
                    profile_name);
            return;
        }

    }

    // pick the client's name (isn't mandatory to provide it). Takes precedence over profile provided.
    const char* name = simple->Attribute(DSxmlparser::NAME);
    if (name != nullptr)
    {
        dpQOS.name() = name;
    }

    GUID_t guid(dpQOS.wire_protocol().prefix, c_EntityId_RTPSParticipant);
    DelayedParticipantDestruction* destruction_event = nullptr;
    DelayedParticipantCreation* creation_event = nullptr;

    if (removal_time != getTime())
    {
        // early leaver
        destruction_event = new DelayedParticipantDestruction(removal_time, guid);
        events.push_back(destruction_event);
    }

    // Create the participant or the associated events
    DelayedParticipantCreation event(creation_time, std::move(dpQOS), &DiscoveryServerManager::addSimple,
            destruction_event);

    if (creation_time == getTime())
    {
        event(*this);

        // get the client guid
        guid = event.participant_guid;
    }
    else
    {
        // late joiner
        creation_event = new DelayedParticipantCreation(std::move(event));
        events.push_back(creation_event);
    }

    // Once the participant is created we create the associated endpoints
    tinyxml2::XMLElement* pub = simple->FirstChildElement(DSxmlparser::PUBLISHER);
    while (pub != nullptr)
    {
        loadPublisher(guid, pub, creation_event);
        pub = pub->NextSiblingElement(DSxmlparser::PUBLISHER);
    }

    tinyxml2::XMLElement* sub = simple->FirstChildElement(DSxmlparser::SUBSCRIBER);
    while (sub != nullptr)
    {
        loadSubscriber(guid, sub, creation_event);
        sub = sub->NextSiblingElement(DSxmlparser::SUBSCRIBER);
    }
}

void DiscoveryServerManager::loadSubscriber(
        GUID_t& part_guid,
        tinyxml2::XMLElement* sub,
        DelayedParticipantCreation* participant_creation_event /*= nullptr*/,
        DelayedParticipantDestruction* participant_destruction_event /*= nullptr*/)
{
    assert(sub != nullptr);

    // check if we need to create an event
    std::chrono::steady_clock::time_point creation_time, removal_time;

    // Match the creation and destruction times to the participant
    if (nullptr != participant_creation_event)
    {
        creation_time = participant_creation_event->executionTime();
        // prevent creation before the participant
        creation_time += std::chrono::nanoseconds(1);
    }
    else
    {
        creation_time = getTime();
    }

    if (nullptr != participant_destruction_event)
    {
        removal_time = participant_creation_event->executionTime();
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

    // see if topic is specified
    const char* topic_name = sub->Attribute(DSxmlparser::TOPIC);
    TopicAttributes topicAttr;
    if (topic_name != nullptr)
    {
        if (!eprosima::fastdds::rtps::RTPSDomain::get_topic_attributes_from_profile(std::string(
                    topic_name), topicAttr))
        {
            LOG_ERROR("DiscoveryServerManager::loadSubscriber couldn't load topic profile ");
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
    std::string endpoint_profile;
    if (profile_name == nullptr)
    {
        endpoint_profile = std::string("");
    }
    else
    {
        endpoint_profile = std::string(profile_name);
    }

    DelayedEndpointCreation<DataReader> event(creation_time, topicAttr.getTopicName().to_string(),
            topicAttr.getTopicDataType().to_string(), topic_name, endpoint_profile, part_guid, pDE,
            participant_creation_event);

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

void DiscoveryServerManager::loadPublisher(
        GUID_t& part_guid,
        tinyxml2::XMLElement* sub,
        DelayedParticipantCreation* participant_creation_event /*= nullptr*/,
        DelayedParticipantDestruction* participant_destruction_event /*= nullptr*/)
{
    assert(sub != nullptr);

    // check if we need to create an event
    std::chrono::steady_clock::time_point creation_time, removal_time;

    // Match the creation and destruction times to the participant
    if ( nullptr != participant_creation_event)
    {
        creation_time = participant_creation_event->executionTime();
        // prevent creation before the participant
        creation_time += std::chrono::nanoseconds(1);
    }
    else
    {
        creation_time = getTime();
    }

    if (nullptr != participant_destruction_event)
    {
        removal_time = participant_creation_event->executionTime();
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

    // see if topic is specified
    const char* topic_name = sub->Attribute(DSxmlparser::TOPIC);
    TopicAttributes topicAttr;
    if (topic_name != nullptr)
    {
        if (!eprosima::fastdds::rtps::RTPSDomain::get_topic_attributes_from_profile(std::string(
                    topic_name), topicAttr))
        {
            LOG_ERROR("DiscoveryServerManager::loadPublisher couldn't load topic profile ");
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

    std::string endpoint_profile;
    if (profile_name == nullptr)
    {
        endpoint_profile = std::string("");
    }
    else
    {
        endpoint_profile = std::string(profile_name);
    }

    DelayedEndpointCreation<DataWriter> event(creation_time, topicAttr.getTopicName().to_string(),
            topicAttr.getTopicDataType().to_string(), topic_name, endpoint_profile, part_guid, pDE,
            participant_creation_event);

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

std::chrono::steady_clock::time_point DiscoveryServerManager::getTime() const
{
    return state.getTime();
}

void DiscoveryServerManager::loadSnapshot(
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

void DiscoveryServerManager::loadEnvironmentChange(
        tinyxml2::XMLElement* change)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    // snapshots are created for debugging purposes
    // time is mandatory
    const char* time_str = change->Attribute(s_sTime.c_str());

    if (time_str == nullptr)
    {
        LOG_ERROR(s_sTime << " is a mandatory attribute of " << s_sChange << " tag");
        return;
    }

    std::chrono::steady_clock::time_point time(getTime());
    {
        int aux;
        std::istringstream(time_str) >> aux;
        time += std::chrono::seconds(aux);
    }

    const char* key = change->Attribute(s_sKey.c_str());
    if (key == nullptr)
    {
        LOG_ERROR(s_sKey << " is a mandatory attribute of " << s_sChange << " tag");
        return;
    }
    const char* value = change->GetText();

    // Add the event
    events.push_back(new DelayedEnvironmentModification(time, key, value));
}

void DiscoveryServerManager::MapServerInfo(
        tinyxml2::XMLElement* server)
{
    std::lock_guard<std::recursive_mutex> lock(management_mutex);

    // profile name is mandatory
    std::string profile_name(server->Attribute(DSxmlparser::PROFILE_NAME));

    if (profile_name.empty())
    {
        // it doesn't log as error because may be empty on purpose
        LOG_INFO(DSxmlparser::PROFILE_NAME << " is a mandatory attribute of server tag");
        return;
    }

    // server GuidPrefix is either pass as an attribute (preferred to allow profile reuse)
    // or inside the profile.
    GuidPrefix_t prefix;

    std::shared_ptr<DomainParticipantQos> pqos;
    // I must load the prefix from the profile
    // retrieve profile QOS
    pqos = std::make_shared<DomainParticipantQos>();
    if (RETCODE_OK !=
            DomainParticipantFactory::get_instance()->get_participant_qos_from_profile(profile_name, *pqos))
    {
        LOG_ERROR("DiscoveryServerManager::loadServer couldn't load profile " << profile_name);
        return;
    }

    const char* cprefix = server->Attribute(DSxmlparser::PREFIX);

    if (cprefix != nullptr)
    {
        std::istringstream(cprefix) >> prefix;
    }
    else
    {
        prefix = pqos->wire_protocol().prefix;
    }

    if (prefix == c_GuidPrefix_Unknown)
    {
        LOG_INFO("Guidless server, locators must be set directly from the XML");
        return;
    }

    // Now we search the locator lists
    serverLocator_map::mapped_type pair;

    pair.first =  pqos->wire_protocol().builtin.metatrafficMulticastLocatorList;
    pair.second = pqos->wire_protocol().builtin.metatrafficUnicastLocatorList;

    // now save the value
    server_locators[GUID_t(prefix, c_EntityId_RTPSParticipant)] = std::move(pair);
}

void DiscoveryServerManager::on_participant_discovery(
        DomainParticipant* participant,
        ParticipantDiscoveryInfo&& info,
        bool& should_be_ignored)
{
    bool server = false;
    const GUID_t& partid = info.info.m_guid;
    static_cast<void>(should_be_ignored);

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
            // DiscoveryServerManager info still valid
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

void DiscoveryServerManager::on_data_reader_discovery(
        DomainParticipant* participant,
        ReaderDiscoveryInfo&& info,
        bool& should_be_ignored)
{
    static_cast<void>(should_be_ignored);
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
            // stick to non-DiscoveryServerManager info
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
        std::ostringstream ss;
        ss << partid;
        part_name = ss.str();
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

void DiscoveryServerManager::on_data_writer_discovery(
        DomainParticipant* participant,
        WriterDiscoveryInfo&& info,
        bool& should_be_ignored)
{
    static_cast<void>(should_be_ignored);
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
            // stick to non-DiscoveryServerManager info
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
        std::ostringstream ss;
        ss << partid;
        part_name = ss.str();
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

void DiscoveryServerManager::on_liveliness_changed(
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

bool DiscoveryServerManager::allKnowEachOther() const
{
    // Get a copy of current state
    Snapshot shot = state.GetState();
    return allKnowEachOther(shot);
}

Snapshot& DiscoveryServerManager::takeSnapshot(
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
bool DiscoveryServerManager::allKnowEachOther(
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

bool DiscoveryServerManager::validateAllSnapshots() const
{
    // traverse the list of snapshots validating then
    bool work_it_all = true;

    for (const Snapshot& sh : snapshots)
    {
        if (DiscoveryServerManager::allKnowEachOther(sh))
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

std::string DiscoveryServerManager::successMessage()
{
    if (snapshots.empty())
    {
        // direct run
        return "Discovery Server run succeeded!";
    }

    return "Output file validation succeeded!";
}

bool DiscoveryServerManager::loadSnapshots(
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

void DiscoveryServerManager::saveSnapshots(
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
