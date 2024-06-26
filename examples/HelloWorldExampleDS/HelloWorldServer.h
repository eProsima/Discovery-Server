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

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/rtps/common/Locator.hpp>

namespace eprosima {
namespace fastdds {
namespace dds {
class DomainParticipant;
} // namespace dds
} // namespace fastdds
} // namespace eprosima

class HelloWorldServer
{
public:

    HelloWorldServer();
    virtual ~HelloWorldServer();
    //!Initialize the subscriber
    bool init(
            eprosima::fastdds::rtps::Locator server_address);
    //!RUN the subscriber
    void run();

private:

    eprosima::fastdds::dds::DomainParticipant* mp_participant;
};

#endif /* HELLOWORLDSERVER_H_ */
