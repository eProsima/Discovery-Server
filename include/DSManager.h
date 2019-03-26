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

#include <fastrtps/rtps/participant/RTPSParticipant.h>

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

class DSManager
{
    typedef std::map<GUID_t, RTPSParticipant*> participant_map;

    participant_map servers;
    participant_map clients;

    bool active;
    void parseProperties(tinyxml2::XMLElement *parent_element,
        std::vector<std::pair<std::string, std::string>> &props);
    void loadProfiles(tinyxml2::XMLElement *profiles);
    void loadServer(tinyxml2::XMLElement* server);
    void loadClient(tinyxml2::XMLElement* client);

public:
    DSManager(const std::string &xml_file_path);
    ~DSManager();
    bool isActive();
    void addServer(RTPSParticipant* b);
    void addClient(RTPSParticipant* p);

    void createReader(RTPSParticipant* participant, const std::string &participant_profile, const std::string &name);
    void createWriter(RTPSParticipant* participant, const std::string &participant_profile, const std::string &name);

    void onTerminate();

    std::string getEndPointName(const std::string &partName, const std::string &epName)
    {
        return partName + "." + epName;
    }

    static PDP * createPDPServer(BuiltinProtocols *);
    static void ReleasePDPServer(PDP *);
};

#endif // _DSMANAGER_H_