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

#include "HelloWorldPubSubTypes.hpp"

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>

#include "HelloWorld.hpp"

class HelloWorldPublisher
{
public:

    HelloWorldPublisher();
    virtual ~HelloWorldPublisher();
    //!Initialize
    bool init(
            eprosima::fastdds::rtps::Locator server_address);
    //!Publish a sample
    bool publish(
            bool waitForListener = true);
    //!Run for number samples
    void run(
            uint32_t number,
            uint32_t sleep);

private:

    HelloWorld m_hello;
    eprosima::fastdds::dds::DomainParticipant* mp_participant;
    eprosima::fastdds::dds::Publisher*  mp_publisher;
    eprosima::fastdds::dds::DataWriter* mp_writer;

    bool stop;
    class PubListener : public eprosima::fastdds::dds::DataWriterListener
    {
    public:

        PubListener()
            : n_matched(0)
        {
        }

        ~PubListener()
        {
        }

        void on_publication_matched(
                eprosima::fastdds::dds::DataWriter* dataWriter,
                const eprosima::fastdds::dds::PublicationMatchedStatus& info);
        int n_matched;
    }
    m_listener;
    void runThread(
            uint32_t number,
            uint32_t sleep);
};

#endif /* HELLOWORLDPUBLISHER_H_ */
