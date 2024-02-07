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


#ifndef _DSMANAGER_H_
#define _DSMANAGER_H_

#include <chrono>
#include <iostream>
#include <map>
#include <regex>
#include <vector>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>

#include <fastrtps/xmlparser/XMLParser.h>

#include "DiscoveryItem.h"

using namespace eprosima::fastrtps;
using namespace eprosima::fastdds;
using namespace eprosima::fastrtps::rtps;
using namespace eprosima::fastdds::rtps;
using namespace eprosima::fastdds::dds;

namespace tinyxml2 {
class XMLElement;
} // namespace tinyxml2

namespace eprosima {
namespace fastrtps {
namespace rtps {

class PDP;
class BuiltinProtocols;

} // namespace rtps
} // namespace fastrtps

namespace discovery_server {

struct ParticipantCreatedEntityInfo
{
    DomainParticipant* participant;
    fastdds::dds::Publisher* publisher;
    fastdds::dds::Topic* publisherTopic;
    fastdds::dds::TopicDataType* publisherType;
    fastdds::dds::Subscriber* subscriber;
    fastdds::dds::Topic* subscriberTopic;
    fastdds::dds::TopicDataType* subscriberType;
    std::map<std::string, fastdds::dds::Topic*> registeredTopics;

    ParticipantCreatedEntityInfo() = default;
    ParticipantCreatedEntityInfo(
            DomainParticipant* p,
            fastdds::dds::Publisher* pub,
            fastdds::dds::Topic* pTopic,
            fastdds::dds::Subscriber* sub,
            fastdds::dds::Topic* sTopic)
        : participant(p)
        , publisher(pub)
        , publisherTopic(pTopic)
        , subscriber(sub)
        , subscriberTopic(sTopic)
    {
    }

};

class LateJoinerData;
class DelayedParticipantCreation;
class DelayedParticipantDestruction;

class DiscoveryServerManager
    : public xmlparser::XMLParser      // access to parsing protected functions
    , public eprosima::fastdds::dds::DomainParticipantListener // receive discovery callback information and
                                                               // subscriber lifeliness information

{
    typedef std::map<GUID_t, DomainParticipant*> participant_map;
    typedef std::map<GUID_t, DataReader*> data_reader_map;
    typedef std::map<GUID_t, DataWriter*> data_writer_map;
    typedef std::map<GUID_t, std::pair<LocatorList_t, LocatorList_t>> serverLocator_map;  // multi, unicast locator list
    typedef std::vector<LateJoinerData*> event_list;
    typedef std::vector<Snapshot> snapshots_list;

    typedef std::map<GUID_t, ParticipantCreatedEntityInfo> created_entity_map;

    // synch protection
    std::recursive_mutex management_mutex;

    // Participant maps
    participant_map servers;
    participant_map clients;
    participant_map simples;

    // Map to hold the information regarding Participants and their associated Publishers, Subscribers and Topics
    // Indexed by GUID
    created_entity_map entity_map;

    // endpoints maps
    data_reader_map data_readers;
    data_writer_map data_writers;

    // server address info
    serverLocator_map server_locators;

    // Discovery status
    DiscoveryItemDatabase state;
    std::chrono::steady_clock::time_point getTime() const;

    // Event list for late joiner creation, destruction and take snapshots
    // only modified from the main thread (no synchronization required)
    event_list events;

    // Snapshops container
    snapshots_list snapshots;

    volatile bool no_callbacks;      // ongoing participant destruction
    bool auto_shutdown;         // close when event processing is finished?
    bool enable_prefix_validation; // allow multiple servers share the same prefix? (only for testing purposes)
    bool correctly_created_;     // store false if the DiscoveryServerManager has not been successfully created

    void loadProfiles(
            tinyxml2::XMLElement* profiles);
    void loadServer(
            tinyxml2::XMLElement* server);
    void loadClient(
            tinyxml2::XMLElement* client);
    void loadSimple(
            tinyxml2::XMLElement* simple);

    void loadSubscriber(
            GUID_t& part_guid,
            tinyxml2::XMLElement* subs,
            DelayedParticipantCreation* participant_creation_event = nullptr,
            DelayedParticipantDestruction* participant_destruction_event = nullptr);

    void loadPublisher(
            GUID_t& part_guid,
            tinyxml2::XMLElement* pubs,
            DelayedParticipantCreation* participant_creation_event = nullptr,
            DelayedParticipantDestruction* participant_destruction_event = nullptr);

    void loadSnapshot(
            tinyxml2::XMLElement* snapshot);
    void loadEnvironmentChange(
            tinyxml2::XMLElement* snapshot);
    void MapServerInfo(
            tinyxml2::XMLElement* server);

    bool loadSnapshots(
            const std::string& file);
    void saveSnapshots(
            const std::string& file) const;

    // File where to save snapshots
    std::string snapshots_output_file;
    // validation required
    bool validate_{false};
    // last callback recorded time
    std::chrono::steady_clock::time_point last_PDP_callback_;
    std::chrono::steady_clock::time_point last_EDP_callback_;
    // last snapshot delay, needed for sync purposes
    static const std::chrono::seconds last_snapshot_delay_;

    bool shared_memory_off_;

public:

    DiscoveryServerManager(
            const std::string& xml_file_path,
            const bool shared_memory_off);

    ~DiscoveryServerManager();

    // testing database
    bool shouldValidate() const
    {
        return validate_ && !snapshots.empty();
    }

    bool validateAllSnapshots() const;
    bool allKnowEachOther() const;
    static bool allKnowEachOther(
            const Snapshot& shot);
    Snapshot&  takeSnapshot(
            const std::chrono::steady_clock::time_point tp,
            const std::string& desc = std::string(),
            bool someone = true,
            bool show_liveliness = false);

    // success message depends on run type
    std::string successMessage();

    // processing events
    void runEvents(
        std::istream& in = std::cin,
        std::ostream& out = std::cout);

    // update entity state functions
    void addServer(
            DomainParticipant* b);
    void addClient(
            DomainParticipant* p);
    void addSimple(
            DomainParticipant* s);
    void addDataReader(
            DataReader*);
    void addDataWriter(
            DataWriter*);

    DomainParticipant* getParticipant(
            GUID_t& id);
    DataReader* getDataReader(
            GUID_t& id);
    DataWriter* getDataWriter(
            GUID_t& id);

    DomainParticipant* removeParticipant(
            GUID_t& id);
    DataReader* removeSubscriber(
            GUID_t& id);
    DataWriter* removePublisher(
            GUID_t& id);

    ReturnCode_t deleteParticipant(
            DomainParticipant* participant);
    ReturnCode_t deleteDataReader(
            DataReader* dr);
    ReturnCode_t deleteDataWriter(
            DataWriter* dw);

    template <class Entity>
    void setDomainEntityTopic(
            Entity* entity,
            Topic* topic);

    template <class Entity>
    void setDomainEntityType(
            Entity* entity,
            TopicDataType* topic);

    void setParticipantInfo(
            GUID_t&,
            ParticipantCreatedEntityInfo& info);

    void setParticipantTopic(
            DomainParticipant* p,
            Topic* t);

    Topic* getParticipantTopicByName(
            DomainParticipant* p,
            const std::string& name);

    void setParentGUID(
            GUID_t& parent,
            GUID_t& child);
    GUID_t getParentGUID(
            GUID_t& child);

    template<class ReaderWriter>
    void getPubSubEntityFromParticipantGuid(
            GUID_t& id,
            DomainEntity*& pubsub);

    // callback discovery functions
    void on_participant_discovery(
            DomainParticipant* participant,
            ParticipantDiscoveryInfo&& info) override;

    void on_subscriber_discovery(
            DomainParticipant* participant,
            ReaderDiscoveryInfo&& info) override;

    void on_publisher_discovery(
            DomainParticipant* participant,
            WriterDiscoveryInfo&& info) override;

    // callback liveliness functions
    void on_liveliness_changed(
            DataReader* sub,
            const LivelinessChangedStatus& status) override;

    void onTerminate();

    std::string getEndPointName(
            const std::string& participant_name,
            const std::string& endpoint_name)
    {
        return participant_name + "." + endpoint_name;
    }

    bool correctly_created()
    {
        return correctly_created_;
    }

    // default topic
    static TopicAttributes builtin_defaultTopic;

    // parsing regex
    static const std::regex ipv4_regular_expression;

    void disable_shared_memory()
    {
        shared_memory_off_ = true;
    }

    bool get_shared_memory_status()
    {
        return shared_memory_off_;
    }

    void output_file(
            std::string file_path)
    {
        snapshots_output_file = file_path;
    }

};

std::ostream& operator <<(
        std::ostream&,
        ParticipantDiscoveryInfo::DISCOVERY_STATUS);
std::ostream& operator <<(
        std::ostream&,
        ReaderDiscoveryInfo::DISCOVERY_STATUS);
std::ostream& operator <<(
        std::ostream&,
        WriterDiscoveryInfo::DISCOVERY_STATUS);

} // namespace discovery_server
} // namespace eprosima

#endif // _DSMANAGER_H_
