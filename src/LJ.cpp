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

#include "log/DSLog.h"
#include "LJ.h"
#include "DSManager.h"

using namespace eprosima::fastrtps;
using namespace eprosima::discovery_server;

// delayed creation of a new participant
void DPC::operator()(
        DSManager& man ) /*override*/
{
    Participant * p = Domain::createParticipant(attributes, &man);
    if (p)
    {
        (man.*participant_creation_function)(p); // addServer or addClient
        participant_guid = p->getGuid();
        // update the associated DPD
        if (removal_event)
        {
            removal_event->SetGuid(participant_guid);
        }

        LOG_INFO("New participant called " << attributes.rtps.getName() << " with prefix " << p->getGuid() );
    }
    else
    {
        LOG_ERROR("DSManager couldn't create the participant " << attributes.rtps.prefix  );
    }
}

// delayed destruction of a new participant
void DPD::operator()(
        DSManager& man) /*override*/
{
    Participant * p = man.removeParticipant(participant_id);
    if (p)
    {
        std::string name = p->getAttributes().rtps.getName();

        Domain::removeParticipant(p);

        LOG_INFO("Removed participant called " << name << " with prefix " << participant_id );
    }
}

void DPD::SetGuid(
        const GUID_t& id)
{
    if (id != GUID_t::unknown())
    {
        participant_id = id; // update
    }
}

// static LJD_atts pointer to member
const std::string LJD_traits<Publisher>::endpoint_type("Publisher");
const LJD_traits<Publisher>::AddEndpoint LJD_traits<Publisher>::add_endpoint_function = &DSManager::addPublisher;
const LJD_traits<Publisher>::GetEndpoint LJD_traits<Publisher>::retrieve_endpoint_function = &DSManager::removePublisher;
const LJD_traits<Publisher>::removeEndpoint LJD_traits<Publisher>::remove_endpoint_function = &Domain::removePublisher;

/*static*/ 
Publisher * LJD_traits<Publisher>::createEndpoint(
    Participant* part,
    const PublisherAttributes& pa,
    void *)
{
    return Domain::createPublisher(part, pa);
}

const std::string LJD_traits<Subscriber>::endpoint_type("Subscriber");
const LJD_traits<Subscriber>::AddEndpoint LJD_traits<Subscriber>::add_endpoint_function = &DSManager::addSubscriber;
const LJD_traits<Subscriber>::GetEndpoint LJD_traits<Subscriber>::retrieve_endpoint_function = &DSManager::removeSubscriber;
const LJD_traits<Subscriber>::removeEndpoint LJD_traits<Subscriber>::remove_endpoint_function = &Domain::removeSubscriber;

/*static*/
Subscriber * LJD_traits<Subscriber>::createEndpoint(
    Participant* part,
    const SubscriberAttributes& sa,
    SubscriberListener * list/* = nullptr*/)
{
    return Domain::createSubscriber(part, sa, list);
}

void DS::operator()(
        DSManager & man) /*override*/
{
    man.takeSnapshot(std::chrono::steady_clock::now(), description, if_someone, show_liveliness_);
}