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

/**
 * @file HelloWorldServer.h
 *
 */

#ifndef HELLOWORLDSERVER_H_
#define HELLOWORLDSERVER_H_

#include <fastdds/dds/domain/DomainParticipantListener.hpp>
#include <fastdds/rtps/common/Locator.h>

namespace eprosima
{
    namespace fastdds
    {
        namespace dds
        {
            class DomainParticipant;
        }
    }
}

class HelloWorldServer
{
public:
    HelloWorldServer();
    virtual ~HelloWorldServer();
    //!Initialize the subscriber
    bool init(eprosima::fastdds::rtps::Locator server_address);
    //!RUN the subscriber
    void run();

private:
    class PubListener :public eprosima::fastdds::dds::DomainParticipantListener
    {
    public:
        PubListener() :n_matched(0), firstConnected(false) {};
        ~PubListener() {};
        void on_participant_discovery(eprosima::fastdds::dds::DomainParticipant* participant, eprosima::fastrtps::rtps::ParticipantDiscoveryInfo&& info);
        int n_matched;
        bool firstConnected;
    } * m_listener;
    eprosima::fastdds::dds::DomainParticipant* mp_participant;
};

#endif /* HELLOWORLDSERVER_H_ */
