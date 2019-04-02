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

#include <cassert>
#include <algorithm>
#include "DI.h"

using namespace eprosima::fastrtps;

// basic discovery items operations

bool DI::operator==(const rtps::GUID_t & guid) const
{
    return id == guid;
}

bool DI::operator==(const DI & d) const
{
    return id == d.id;
}

bool DI::operator<(const rtps::GUID_t & guid) const
{
    return id < guid;
}

bool DI::operator<(const DI & d) const
{
    return id < d.id;
}

// publiser discovery item operations
bool PDI::operator==(const PDI & p) const
{
    assert(_typeName == p._typeName);
    assert(_topicName == p._topicName);

    return DI::operator==(p);
}

// subscriber discovery item operations
bool SDI::operator==(const SDI & p) const
{
    assert(_typeName == p._typeName);
    assert(_topicName == p._topicName);

    return DI::operator==(p);
}

// participant discovery item operations

bool PtDI::operator==(const PtDI &) const
{
    // this is an ugly one to implement
    return true;
}

bool PtDI::operator[](const PtDI & p) const
{
    // PtDI contains itself 
    if (DI::operator==(p.id))
        return true;

    // search the list
    return _participants.end() != _participants.find(p);
}

bool PtDI::operator[](const PDI & p) const
{
    // search the list
    return _publishers.end() != _publishers.find(p);
}

bool PtDI::operator[](const SDI & p) const
{
    // search the list
    return _subscribers.end() != _subscribers.find(p);
}

// DI_database methods

bool DI_database::AddParticipant(const rtps::GUID_t ptid, bool server/* = false*/)
{
    std::lock_guard<std::mutex> lock(_mtx);

    auto it = std::find_if(_database.begin(), _database.end(), 
        [&ptid](const PtDI & p) { return p == ptid; });

    

    return true;

}

bool DI_database::RemoveParticipant(const rtps::GUID_t ptid)
{
    return false;
}

bool DI_database::AddSubscriber(const rtps::GUID_t ptid, const rtps::GUID_t sid, const std::string typename, const std::string topicname)
{
    return false;
}

bool DI_database::RemoveSubscriber(const rtps::GUID_t sid)
{
    return false;
}

bool DI_database::AddPublisher(const rtps::GUID_t ptid, const rtps::GUID_t pid, const std::string typename, const std::string topicname)
{
    return false;
}

bool DI_database::RemovePublisher(const rtps::GUID_t ptid)
{
    return false;
}

size_t DI_database::CountParticipants() const
{
    return _database.size();
}

size_t DI_database::CountSubscribers() const
{
    return 0;
}

size_t DI_database::CountPublishers() const
{
    return 0;
}

size_t DI_database::CountSubscribers(const rtps::GUID_t ptid) const
{
    return 0;
}

size_t DI_database::CountPublishers(const rtps::GUID_t ptid) const
{
    return 0;
}