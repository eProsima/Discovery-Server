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


#ifndef _DI_H_
#define _DI_H_

#include <fastrtps/rtps/common/Guid.h>
#include <set>
#include <mutex>
#include <chrono>

namespace eprosima {
    namespace fastrtps {

        //! common discovery info
        struct DI
        {
            //! enpoint identifier
            rtps::GUID_t id;

            //! comparissons
            bool operator==(const rtps::GUID_t &) const;
            bool operator==(const DI &) const;

            //! container ancillary
            bool operator<(const rtps::GUID_t &) const;
            bool operator<(const DI &) const;
        };

        //! publisher specific info
        struct PDI : public DI
        {
            //!Type name
            std::string _typeName;

            //!Topic name
            std::string _topicName;

            //! comparissons
            bool operator==(const PDI &) const;
        };

        //! subscriber specific info
        struct SDI : public DI
        {
            //!Type name
            std::string _typeName;

            //!Topic name
            std::string _topicName;

            //! comparissons
            bool operator==(const SDI &) const;
        };

        //! participant discovery info
        struct PtDI : public DI
        {
            // identity
            bool _server; // false -> client
            bool _alive; // false if dead reported

            // local user entities
            std::set<PDI> _publishers;
            std::set<SDI> _subscribers;

            // network discovery information
            std::set<PtDI> _participants;

            // comparissons:

            using DI::operator==;

            /**
            * verifies if two PtDI keep the same info
            * despite been rooted on two different participants
            **/
            bool operator==(const PtDI &) const;

            //! verifies if the given participant was discovered by the participant
            bool operator[](const PtDI &) const;

            //! verifies if the given publisher was discovered by the participant
            bool operator[](const PDI &) const;

            //! verifies if the given subscriber was discovered by the participant
            bool operator[](const SDI &) const;

        };


        class DI_database
        {
            typedef std::set<PtDI> database;

            typedef struct 
            {
                // snapshot time
                std::chrono::high_resolution_clock::time_point _time;
                database _data;

            } Snapshot;

            // reported discovery info
            database _database;
            std::mutex _mtx; // atomic database operation

        public:
            DI_database() {}

            bool AddParticipant(const rtps::GUID_t ptid,bool server = false);
            bool RemoveParticipant(const rtps::GUID_t ptid);

            bool AddSubscriber(const rtps::GUID_t ptid, const rtps::GUID_t sid, const std::string typename, const std::string topicname);
            bool RemoveSubscriber(const rtps::GUID_t sid);

            bool AddPublisher(const rtps::GUID_t ptid, const rtps::GUID_t pid, const std::string typename, const std::string topicname);
            bool RemovePublisher(const rtps::GUID_t ptid);

            size_t CountParticipants() const;
            size_t CountSubscribers() const;
            size_t CountPublishers() const;
            size_t CountSubscribers(const rtps::GUID_t ptid) const;
            size_t CountPublishers(const rtps::GUID_t ptid) const;

        };

    } // fastrtps
} // eprosima

#endif // _DI_H_
