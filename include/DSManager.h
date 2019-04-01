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
#include <iostream>
#include "log/DSLog.h"

#include <fastrtps/participant/Participant.h>
#include <fastrtps/participant/ParticipantListener.h>
#include <fastrtps/xmlparser/XMLParser.h>

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
}

class DSManager : public xmlparser::XMLParser,      // access to parsing protected functions
    public eprosima::fastrtps::ParticipantListener  // receive discovery callback information
{
    typedef std::map<GUID_t, Participant*> participant_map;
    typedef std::map<GUID_t, std::pair<LocatorList_t, LocatorList_t> > serverLocator_map; // multi, unicast locator list

    participant_map _servers;
    participant_map _clients;

    serverLocator_map _server_locators;

    bool _active;
    std::recursive_mutex _mutex;

    void loadProfiles(tinyxml2::XMLElement *profiles);
    void loadServer(tinyxml2::XMLElement* server);
    void loadClient(tinyxml2::XMLElement* client);
    void MapServerInfo(tinyxml2::XMLElement* server);

public:
    DSManager(const std::string &xml_file_path);
    ~DSManager();
    bool isActive();

    // entity creation functions
    void addServer(Participant* b);
    void addClient(Participant* p);
    void addSubscriber(Participant* participant, const std::string &participant_profile, const std::string &name) {}; // TODO: implementation
    void addPublisher(Participant* participant, const std::string &participant_profile, const std::string &name) {};  // TODO: implementation

    // callback discovery functions
    void onParticipantDiscovery(Participant* participant, rtps::ParticipantDiscoveryInfo&& info) override;
    void onSubscriberDiscovery(Participant* participant, rtps::ReaderDiscoveryInfo&& info) override;
    void onPublisherDiscovery(Participant* participant, rtps::WriterDiscoveryInfo&& info) override;

    void onTerminate();

    std::string getEndPointName(const std::string &partName, const std::string &epName)
    {
        return partName + "." + epName;
    }

    template<bool persist> static PDP * createPDPServer(BuiltinProtocols *);
    static void ReleasePDPServer(PDP *);
};


std::ostream& operator<<(std::ostream&, ParticipantDiscoveryInfo::DISCOVERY_STATUS );


#endif // _DSMANAGER_H_
