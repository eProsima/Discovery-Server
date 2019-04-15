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


#include "LJ.h"
#include "DSManager.h"

using namespace eprosima::fastrtps;
using namespace eprosima::discovery_server;

// delayed creation of a new participant
void DPC::operator()(DSManager & man ) /*override*/
{
    Participant * p = Domain::createParticipant(_atts, &man);
    if (p)
    {
        (man.*_m)(p); // addServer or addClient
        _guid = p->getGuid();
        // update the associated DPD
        if (_pD)
        {
            _pD->SetGuid(_guid);
        }

        LOG_INFO("New participant called " << _atts.rtps.getName() << " with prefix " << p->getGuid() );
    }
    else
    {
        LOG_ERROR("DSManager couldn't create the participant " << _atts.rtps.prefix  );
    }
}

// delayed destruction of a new participant
void DPD::operator()(DSManager & man) /*override*/
{
    Participant * p = man.removeParticipant(_id);
    if (p)
    {
        std::string name = p->getAttributes().rtps.getName();

        Domain::removeParticipant(p);

        LOG_INFO("Removed participant called " << name << " with prefix " << _id );
    }
}

void DPD::SetGuid(const GUID_t& id)
{
    if (id != GUID_t::unknown())
    {
        _id = id; // update
    }
}

// static LJD_atts pointer to member
const std::string LJD_traits<Publisher>::_endpoint_type("Publisher");
const LJD_traits<Publisher>::addEndpoint LJD_traits<Publisher>::_ae = &DSManager::addPublisher;
const LJD_traits<Publisher>::getEndpoint LJD_traits<Publisher>::_ge = &DSManager::removePublisher;
const LJD_traits<Publisher>::createEndpoint LJD_traits<Publisher>::_ce = &Domain::createPublisher;
const LJD_traits<Publisher>::removeEndpoint LJD_traits<Publisher>::_re = &Domain::removePublisher;

const std::string LJD_traits<Subscriber>::_endpoint_type("Subscriber");
const LJD_traits<Subscriber>::addEndpoint LJD_traits<Subscriber>::_ae = &DSManager::addSubscriber;
const LJD_traits<Subscriber>::getEndpoint LJD_traits<Subscriber>::_ge = &DSManager::removeSubscriber;
const LJD_traits<Subscriber>::createEndpoint LJD_traits<Subscriber>::_ce = &Domain::createSubscriber;
const LJD_traits<Subscriber>::removeEndpoint LJD_traits<Subscriber>::_re = &Domain::removeSubscriber;

void DS::operator()(DSManager & man) /*override*/
{
    man.takeSnapshot(std::chrono::steady_clock::now(), _desc);
}