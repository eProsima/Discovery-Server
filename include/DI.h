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
    namespace discovery_server {

        typedef fastrtps::rtps::GUID_t GUID_t;

        //! common discovery info
        struct DI
        {
            typedef std::set<DI>::size_type size_type;

            //! enpoint identifier
            GUID_t _id;
        
            DI(const GUID_t& id) : _id(id) {}
            DI(GUID_t&& id) : _id(std::move(id)) {}
            DI(const DI&) = default;

            DI() = delete;
            DI(DI&&) = default;
            DI& operator=(const DI&) = default;
            DI& operator=(DI&&) = default;

            //! comparissons
            bool operator==(const GUID_t&) const;
            bool operator!=(const GUID_t&) const;
            bool operator==(const DI&) const;

            //! container ancillary
            bool operator<(const GUID_t&) const;
            bool operator<(const DI&) const;
        };

        //! publisher specific info
        struct PDI : public DI
        {
            PDI(const GUID_t& id, const std::string& type, const std::string& topic) :
                DI(id), _typeName(type), _topicName(topic) {}

            PDI(GUID_t&& id, std::string&& type, std::string&& topic) :
                DI(id), _typeName(std::move(type)), _topicName(std::move(topic)) {}

            PDI() = delete;
            PDI(const PDI&) = default;
            PDI(PDI&&) = default;
            PDI& operator=(const PDI&) = default;
            PDI& operator=(PDI&&) = default;

            //!Type name
            std::string _typeName;

            //!Topic name
            std::string _topicName;

            //! comparissons
            bool operator==(const PDI&) const;
        };

        //! subscriber specific info
        struct SDI : public DI
        {
            SDI(const GUID_t& id, const std::string& type, const std::string& topic) :
                DI(id), _typeName(type), _topicName(topic) {}

            SDI(GUID_t&& id, std::string&& type, std::string&& topic) :
                DI(id), _typeName(std::move(type)), _topicName(std::move(topic)) {}

            SDI() = delete;
            SDI(const SDI&) = default;
            SDI(SDI&&) = default;
            SDI& operator=(const SDI&) = default;
            SDI& operator=(SDI&&) = default;

            //!Type name
            std::string _typeName;

            //!Topic name
            std::string _topicName;

            //! comparissons
            bool operator==(const SDI&) const;
        };

        //! participant discovery info
        struct PtDI : public DI
        {
            typedef std::set<PDI> publisher_set;
            typedef std::set<SDI> subscriber_set;

            // identity
            bool _server; // false -> client
            bool _alive; // false if death already reported but owned endpoints yet to be 
            std::string _name;

            // local user entities
            publisher_set _publishers;
            subscriber_set _subscribers;

            PtDI(const GUID_t& id,const std::string& name = std::string(), bool server = false) :
                DI(id), _server(server), _alive(true), _name(name) {}

            PtDI(GUID_t&& id,std::string&& name = std::string(), bool server = false) :
                DI(id), _server(server), _alive(true), _name(name) {}

            PtDI() = delete;
            PtDI(const PtDI&) = default;
            PtDI(PtDI&&) = default;
            PtDI& operator=(const PtDI&) = default;
            PtDI& operator=(PtDI&&) = default;

            // comparissons:

            using DI::operator==;

            /**
            * verifies if two PtDI keep the same info
            * despite been rooted on two different participants
            **/
            bool operator==(const PtDI &) const;

            //! verifies if the given publisher was discovered by the participant
            bool operator[](const PDI &) const;

            //! verifies if the given subscriber was discovered by the participant
            bool operator[](const SDI &) const;

            //! modify death acknowledge state
            void acknowledge(bool alive) const;

            // the get methods allows us to workaround STL constrain on sets
            // that makes all its iterators constant

            //! get publishers
            publisher_set& getPublishers() const 
                { return const_cast<publisher_set &>(_publishers); }

            //! get subscribers
            subscriber_set& getSubscribers() const 
                { return const_cast<subscriber_set&>(_subscribers); }

            void setName(const std::string & name) const { const_cast<std::string&>(_name) = name; }
            void setServer(bool & s) const { const_cast<bool &>(_server) = s; }

            //! Returns the number of endpoints owned
            size_type CountEndpoints() const;

        };

        class DI_database
        {
            typedef std::set<PtDI> database;
            typedef database::size_type size_type;

            typedef struct 
            {
                // snapshot time
                std::chrono::high_resolution_clock::time_point _time;
                database _data;

            } Snapshot;

            // reported discovery info
            database _database;
            mutable std::mutex _mtx; // atomic database operation

            // AddSubscriber and AddPublisher common implementation

            template<class T>
                bool AddEndPoint(T&(PtDI::* m)() const,const GUID_t & ptid, const GUID_t & sid,
                    const std::string & _typename, const std::string & topicname);

            template<class T>
                bool RemoveEndPoint(T&(PtDI::* m)() const, const GUID_t & ptid, const GUID_t & sid);

        public:

            //! Returns a pointer to the PtDI or null if not found
            const PtDI* FindParticipant(const GUID_t & ptid);

            //! Adds a new participant, returns false if allocation fails
            bool AddParticipant(const GUID_t & ptid, const std::string& name = std::string(), bool server = false);
            //! Removes a participant, returns false if no there
            bool RemoveParticipant(const GUID_t & ptid);

            //! Adds a new Subscriber, returns false if allocation fails
            bool AddSubscriber(const GUID_t & ptid, const GUID_t & sid, const std::string& _typename, const std::string & topicname);
            bool RemoveSubscriber(const GUID_t & ptid, const GUID_t & sid);

            //! Adds a new Publisher, returns false if allocation fails
            bool AddPublisher(const GUID_t & ptid, const GUID_t & pid, const std::string & _typename, const std::string & topicname);
            bool RemovePublisher(const GUID_t & ptid, const GUID_t & pid);

            size_type CountParticipants() const;
            size_type CountSubscribers() const;
            size_type CountPublishers() const;
            size_type CountSubscribers(const GUID_t & ptid) const;
            size_type CountPublishers(const GUID_t & ptid) const;

        };

    } // fastrtps
} // discovery_server

#endif // _DI_H_
