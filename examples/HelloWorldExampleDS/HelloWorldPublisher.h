// Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
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
 * @file HelloWorldPublisher.h
 *
 */

#ifndef HELLOWORLDPUBLISHER_H_
#define HELLOWORLDPUBLISHER_H_

#include "HelloWorldPubSubTypes.h"

#include <fastdds/rtps/attributes/WriterAttributes.h>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>


#include "HelloWorld.h"


namespace eprosima
{
    namespace fastdds
    {
        namespace dds
        {
            class Publisher;
        }
    }
}



class HelloWorldPublisher 
{
public:
    HelloWorldPublisher();
    virtual ~HelloWorldPublisher();
    //!Initialize
    bool init(eprosima::fastrtps::rtps::Locator_t server_address);
    //!Publish a sample
    bool publish(bool waitForListener = true);
    //!Run for number samples
    void run(uint32_t number, uint32_t sleep);
private:
    HelloWorld m_hello;
    eprosima::fastdds::dds::DomainParticipant* mp_participant;
    eprosima::fastdds::dds::Publisher*  mp_publisher;
    eprosima::fastdds::dds::DataWriter* mp_writer;


    bool stop;
    class PubListener :public eprosima::fastdds::dds::DomainParticipantListener
    {
    public:
        PubListener() :n_matched(0), firstConnected(false) {};
        ~PubListener() {};
        void on_publication_matched(eprosima::fastdds::dds::DataWriter* dataWriter, eprosima::fastdds::dds::PublicationMatchedStatus& info);   
        void on_participant_discovery(eprosima::fastdds::dds::DomainParticipant* participant, eprosima::fastrtps::rtps::ParticipantDiscoveryInfo& info);
        int n_matched;
        bool firstConnected;
    }m_listener;
    void runThread(uint32_t number, uint32_t sleep);
    HelloWorldPubSubType m_type;
};

//eprosima::fastdds::dds::DataWriterListener, 

#endif /* HELLOWORLDPUBLISHER_H_ */
