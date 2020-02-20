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

#include <map>
#include <vector>
#include <iostream>
#include <regex>
#include <chrono>

#include "log/DSLog.h"

#include <fastrtps/participant/Participant.h>
#include <fastrtps/participant/ParticipantListener.h>
#include <fastrtps/subscriber/SubscriberListener.h>
#include <fastrtps/publisher/PublisherListener.h>
#include <fastrtps/xmlparser/XMLParser.h>

#include "../resources/static_types/HelloWorldPubSubTypes.h"

#include "DI.h"

using namespace eprosima::fastrtps;
using namespace eprosima::fastrtps::rtps;

namespace tinyxml2
{
class XMLElement;
}

namespace eprosima {
namespace fastrtps {
namespace rtps {

    class PDP;
    class BuiltinProtocols;

}
}

namespace discovery_server
{

class LJD;
class DPC;
class DPD;

class DSManager
    : public xmlparser::XMLParser      // access to parsing protected functions
    , public eprosima::fastrtps::ParticipantListener  // receive discovery callback information
    , public eprosima::fastrtps::SubscriberListener  // receive subscriber lifeliness information
{
    typedef std::map<GUID_t, Participant*> participant_map;
    typedef std::map<GUID_t, Subscriber*> subscriber_map;
    typedef std::map<GUID_t, Publisher*> publisher_map;
    typedef std::map<std::string, types::DynamicPubSubType *> type_map;
    typedef std::map<GUID_t, std::pair<LocatorList_t, LocatorList_t> > serverLocator_map; // multi, unicast locator list
    typedef std::vector<LJD*> event_list;
    typedef std::vector<Snapshot> snapshots_list;

    // synch protection
    std::recursive_mutex management_mutex;

    // Participant maps
    participant_map servers;
    participant_map clients;
    participant_map simples;

    // endpoints maps
    subscriber_map subscribers;
    publisher_map publishers;

    // server address info
    serverLocator_map server_locators;

    // Discovery status
    DI_database state;
    std::chrono::steady_clock::time_point getTime() const;

    // Event list for late joiner creation, destruction and take snapshots
    // only modified from the main thread (no synchronization required)
    event_list events;

    // Snapshops container
    snapshots_list snapshots;

    volatile bool no_callbacks;      // ongoing participant destruction
    bool auto_shutdown;         // close when event processing is finished?
    bool enable_prefix_validation; // allow multiple servers share the same prefix? (only for testing purposes)

    void loadProfiles(tinyxml2::XMLElement *profiles);
    void loadServer(tinyxml2::XMLElement* server);
    void loadClient(tinyxml2::XMLElement* client);
    void loadSimple(tinyxml2::XMLElement* simple);

    void loadSubscriber(
        GUID_t & part_guid,
        tinyxml2::XMLElement* subs,
        DPC* pPC = nullptr,
        DPD* pPD = nullptr);

    void loadPublisher(
        GUID_t & part_guid,
        tinyxml2::XMLElement* pubs,
        DPC* pPC = nullptr,
        DPD* pPD = nullptr);

    void loadSnapshot(tinyxml2::XMLElement* snapshot);
    void MapServerInfo(tinyxml2::XMLElement* server);

    bool loadSnapshots(const std::string& file);
    void saveSnapshots(const std::string& file) const;

    // type handling
    type_map loaded_types;

    // File where to save snapshots
    std::string snapshots_output_file;
    // validation required
    bool validate_{false};
    // last callback recorded time
    std::chrono::steady_clock::time_point last_PDP_callback_;
    std::chrono::steady_clock::time_point last_EDP_callback_;
    // last snapshot delay, needed for sync purposes
    static const std::chrono::seconds last_snapshot_delay_;

public:
    DSManager(const std::string& xml_file_path);
    DSManager(const std::set<std::string>& xml_snapshot_files,
        const std::string & output_file);
    ~DSManager();

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
        bool someone = true);

    // success message depends on run type
    std::string successMessage();

    // processing events
    void runEvents(
        std::istream& in = std::cin,
        std::ostream& out = std::cout);

    // update entity state functions
    void addServer(Participant* b);
    void addClient(Participant* p);
    void addSimple(Participant* s);
    void addSubscriber(Subscriber *);
    void addPublisher(Publisher *);

    Participant * getParticipant(GUID_t & id);
    Subscriber * getSubscriber(GUID_t & id);
    Publisher * getPublisher(GUID_t & id);

    Participant * removeParticipant(GUID_t & id);
    Subscriber * removeSubscriber(GUID_t & id);
    Publisher * removePublisher(GUID_t & id);

    types::DynamicPubSubType * getType(std::string & name);
    types::DynamicPubSubType * setType(std::string & name);

    // callback discovery functions
    void onParticipantDiscovery(
        Participant* participant,
        rtps::ParticipantDiscoveryInfo&& info) override;

    void onSubscriberDiscovery(
        Participant* participant,
        rtps::ReaderDiscoveryInfo&& info) override;

    void onPublisherDiscovery(
        Participant* participant,
        rtps::WriterDiscoveryInfo&& info) override;

    // callback liveliness functions
    void on_liveliness_changed(
        Subscriber* sub,
        const LivelinessChangedStatus& status) override;

    void onTerminate();

    std::string getEndPointName(
        const std::string &partName,
        const std::string &epName)
    {
        return partName + "." + epName;
    }

    // default topics
    static HelloWorldPubSubType builtin_defaultType;
    static TopicAttributes builtin_defaultTopic;

    // parsing regex
    static const std::regex ipv4_regular_expression;

};


std::ostream& operator<<(std::ostream&, ParticipantDiscoveryInfo::DISCOVERY_STATUS);
std::ostream& operator<<(std::ostream&, ReaderDiscoveryInfo::DISCOVERY_STATUS);
std::ostream& operator<<(std::ostream&, WriterDiscoveryInfo::DISCOVERY_STATUS);

}
}

#endif // _DSMANAGER_H_
