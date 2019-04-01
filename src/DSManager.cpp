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

#include <tinyxml2.h>

#include <fastrtps/Domain.h>
#include <fastrtps/xmlparser/XMLProfileManager.h>

#include "PDPServer.h"
#include "DSManager.h"

#include <sstream>

using namespace eprosima::fastrtps;

// String literals:
// brand new
static const std::string s_sDS("DS");
static const std::string s_sServers("servers");
static const std::string s_sServer("server");
static const std::string s_sClients("clients");
static const std::string s_sClient("client");
static const std::string s_sPersist("persist");
static const std::string s_sLP("ListeningPorts");
static const std::string s_sSL("ServersList");
static const std::string s_sRServer("RServer");

// non exported from fast-RTPS (watch out they are updated)
namespace eprosima{
    namespace fastrtps{
        namespace xmlparser
        {
            const char* PROFILES = "profiles";
            const char* PROFILE_NAME = "profile_name";
            const char* PREFIX = "prefix";
            const char* NAME = "name";
            const char* META_UNI_LOC_LIST = "metatrafficUnicastLocatorList";
            const char* META_MULTI_LOC_LIST = "metatrafficMulticastLocatorList";
        }
    }
}

DSManager::DSManager(const std::string &xml_file_path)
    : _active(false)
{
    //Log::SetVerbosity(Log::Warning);
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(xml_file_path.c_str()) == tinyxml2::XMLError::XML_SUCCESS)
    {
        tinyxml2::XMLElement *root = doc.FirstChildElement(s_sDS.c_str());
        if (!root)
        {
            LOG("Invalid config file");
            return;
        }

        // TODO: servers and clients parsing calls

        for (auto child = doc.FirstChildElement(s_sDS.c_str());
            child != nullptr; child = child->NextSiblingElement(s_sDS.c_str()))
        {
            // first make XMLProfileManager::loadProfiles parse the config file. Afterwards loaded info is accessible
            // through XMLProfileManager::fillParticipantAttributes and related

            tinyxml2::XMLElement *profiles = child->FirstChildElement(xmlparser::PROFILES);
            if (profiles)
            {
                loadProfiles(profiles);
            }
            else
            {
                LOG_ERROR("No profiles found!");
                return;
            }

            // Server processing requires a two pass analysis
            tinyxml2::XMLElement *servers = child->FirstChildElement(s_sServers.c_str());

            if (servers)
            {
            // 1. First map each server with his locators
                tinyxml2::XMLElement *server = servers->FirstChildElement(s_sServer.c_str());
                while (server)
                {
                    MapServerInfo(server);
                    server = server->NextSiblingElement(s_sServer.c_str());
                }

            // 2. Create the servers according with their configuration

                server = servers->FirstChildElement(s_sServer.c_str());
                while (server)
                {
                    loadServer(server);
                    server = server->NextSiblingElement(s_sServer.c_str());
                }
            }
            else
            {
                LOG_ERROR("No servers found!");
                return;
            }

            // Create the clients according with the configuration, clients have only testing purposes
            tinyxml2::XMLElement *clients = child->FirstChildElement(s_sClients.c_str());
            if (clients)
            {
                tinyxml2::XMLElement *client = clients->FirstChildElement(s_sClient.c_str());
                while (client)
                {
                    loadClient(client);
                    client = client->NextSiblingElement(s_sClient.c_str());
                }
            }

        }

        // at least one server must be created from config file
        if (_servers.size() > 0)
        {
            _active = true;
        }
    }
    else
    {
        LOG("Config file not found.");
    }

    LOG_INFO("File " << xml_file_path << " parsed successfully.");
}

void DSManager::addServer(Participant* s)
{
    assert(_servers[s->getGuid()] == nullptr);
    _servers[s->getGuid()] = s;
    _active = true;
}

void DSManager::addClient(Participant* c)
{
    assert(_clients[c->getGuid()] == nullptr);
    _clients[c->getGuid()] = c;
}

void DSManager::loadProfiles(tinyxml2::XMLElement *profiles)
{
    xmlparser::XMLP_ret ret = xmlparser::XMLProfileManager::loadXMLProfiles(*profiles);

    if (ret == xmlparser::XMLP_ret::XML_OK)
    {
        LOG_INFO("Profiles parsed successfully.");
    }
    else
    {
        LOG_ERROR("Error parsing profiles!");
    }
}

void DSManager::createReader(Participant* participant, const std::string &participant_profile, const std::string &name)
{
    //RTPSSubscriber* listener = new RTPSSubscriber(getEndPointName(participant_profile, name));
    //listener->setParticipant(participant);
    //if(!listener->hasParticipant())
    //{
    //    LOG_ERROR("Error creating RTPS subscriber");
    //    return;
    //}

    //// Check type
    //SubscriberAttributes subscriber_att;
    //if ( xmlparser::XMLP_ret::XML_OK ==
    //     xmlparser::XMLProfileManager::fillSubscriberAttributes(name, subscriber_att))
    //{
    //    std::string topic_type = subscriber_att.topic.getTopicDataType();
    //    TopicDataType *type = nullptr;
    //    if (!Domain::getRegisteredType(participant, topic_type.c_str(), &type))
    //    {
    //        // Type not registered yet.
    //        type = getTopicDataType(topic_type);
    //        if (type != nullptr)
    //        {
    //            Domain::registerType(participant, type);
    //            std::pair<std::string, std::string> idx = std::make_pair(participant_profile, topic_type);
    //            data_types[idx] = type;
    //        }
    //        else
    //        {
    //            LOG_WARN("Cannot determine the TopicDataType of " << name << ".");
    //        }
    //    }
    //}
    //else
    //{
    //    LOG_WARN("Cannot get RTPS subscriber attributes: The subscriber " << name << " creation probably will fail.");
    //}

    //// Create Subscriber
    //eprosima::fastrtps::Subscriber *subscriber =
    //        Domain::createSubscriber(participant, name, (SubscriberListener*)listener);

    //listener->setRTPSSubscriber(subscriber);

    //if(!listener->hasRTPSSubscriber())
    //{
    //    LOG_ERROR("Error creating RTPS subscriber");
    //    return;
    //}

    ////Associate types
    //const std::string &typeName = listener->getRTPSSubscriber()->getAttributes().topic.topicDataType;
    //const std::string &topic_name = listener->getRTPSSubscriber()->getAttributes().topic.topicName;
    //std::pair<std::string, std::string> idx = std::make_pair(participant_profile, typeName);
    //listener->input_type = data_types[idx];
    //listener->input_type->setName(typeName.c_str());

    //addReader(listener);
    //LOG_INFO("Added RTPS subscriber " << listener->getName() << "[" << topic_name << ":"
    //    << participant->getAttributes().rtps.builtin.domainId << "]");
}

void DSManager::createWriter(Participant* participant, const std::string &participant_profile, const std::string &name)
{
    //RTPSPublisher* publisher = new RTPSPublisher(getEndPointName(participant_profile, name));

    //// Create RTPSParticipant
    //publisher->setParticipant(participant);
    //if(!publisher->hasParticipant())
    //{
    //    delete publisher;
    //    LOG_ERROR("Error creating RTPS publisher");
    //    return;
    //}

    //// Check type
    //PublisherAttributes publisher_att;
    //if ( xmlparser::XMLP_ret::XML_OK ==
    //     xmlparser::XMLProfileManager::fillPublisherAttributes(name, publisher_att))
    //{
    //    std::string topic_type = publisher_att.topic.getTopicDataType();
    //    TopicDataType *type = nullptr;
    //    if (!Domain::getRegisteredType(participant, topic_type.c_str(), &type))
    //    {
    //        // Type not registered yet.
    //        type = getTopicDataType(topic_type);
    //        if (type != nullptr)
    //        {
    //            Domain::registerType(participant, type);
    //            std::pair<std::string, std::string> idx = std::make_pair(participant_profile, topic_type);
    //            data_types[idx] = type;
    //        }
    //        else
    //        {
    //            LOG_WARN("Cannot determine the TopicDataType of " << name << ".");
    //        }
    //    }
    //}
    //else
    //{
    //    LOG_WARN("Cannot get RTPS publisher attributes: The publisher " << name << " creation probably will fail.");
    //}

    ////Create publisher
    //publisher->setRTPSPublisher(Domain::createPublisher(publisher->getParticipant(), name,
    //                            (PublisherListener*)publisher));

    ////Associate types
    //const std::string &typeName = publisher->getRTPSPublisher()->getAttributes().topic.topicDataType;
    //const std::string &topic_name = publisher->getRTPSPublisher()->getAttributes().topic.topicName;
    //std::pair<std::string, std::string> idx = std::make_pair(participant_profile, typeName);
    //publisher->output_type = data_types[idx];
    //publisher->output_type->setName(typeName.c_str());

    ////Create publisher
    ////publisher->setRTPSPublisher(Domain::createWriter(publisher->getParticipant(), name,
    ////                            (PublisherListener*)publisher));
    //if(!publisher->hasRTPSPublisher())
    //{
    //    delete publisher;
    //    LOG_ERROR("Error creating RTPS publisher");
    //    return;
    //}

    //addWriter(publisher);
    //LOG_INFO("Added RTPS publisher " << publisher->getName()<< "[" << topic_name << ":"
    //    << participant->getAttributes().rtps.builtin.domainId << "]");
}

void DSManager::parseProperties(tinyxml2::XMLElement *parent_element,
                                std::vector<std::pair<std::string, std::string>> &props)
{
    //tinyxml2::XMLElement *props_element = parent_element->FirstChildElement(s_sProperty.c_str());
    //while (props_element)
    //{
    //    try
    //    {
    //        std::pair<std::string, std::string> newPair;
    //        const char *type = _assignNextElement(props_element, s_sName.c_str())->GetText();
    //        const char *value = _assignNextElement(props_element, s_sValue.c_str())->GetText();
    //        newPair.first = type;
    //        newPair.second = value;
    //        props.emplace_back(newPair);
    //    }
    //    catch (...) {}
    //    props_element = props_element->NextSiblingElement(s_sProperty.c_str());
    //}
}

void DSManager::onTerminate()
{


    _clients.insert(_servers.begin(), _servers.end());

    for (const auto &e : _clients)
    {
        Participant *p = e.second;
        if (p)
        {
            Domain::removeParticipant(p);
        }
    }

    _servers.clear();
    _clients.clear();
}

bool DSManager::isActive()
{
    return _active;
}


DSManager::~DSManager()
{
    onTerminate();
}

// TODO: keep track of allocated objects in order to prevent memory leaks

template<>
/*static*/ PDP * DSManager::createPDPServer<true>(BuiltinProtocols * builtin)
{
    assert(builtin);
    return new PDPServer(builtin,DurabilityKind_t::TRANSIENT);
}

template<>
/*static*/ PDP * DSManager::createPDPServer<false>(BuiltinProtocols * builtin)
{
    assert(builtin);
    return new PDPServer(builtin, DurabilityKind_t::TRANSIENT_LOCAL);
}


/*static*/ void DSManager::ReleasePDPServer(PDP * p)
{
    assert(p);
    delete p;
}

void DSManager::loadServer(tinyxml2::XMLElement* server)
{
    // profile name is mandatory
    std::string profile_name(server->Attribute(xmlparser::PROFILE_NAME));

    if (profile_name.empty())
    {
        LOG_ERROR(xmlparser::PROFILE_NAME << " is a mandatory attribute of server tag");
        return;
    }

    // retrieve profile attributes
    ParticipantAttributes atts;
    if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::fillParticipantAttributes(profile_name, atts))
    {
        LOG_ERROR("DSManager::loadServer couldn't load profile " << profile_name);
        return;
    }

    // server name is either pass as an attribute (preferred to allow profile reuse) or inside the profile
    std::string name(server->Attribute(xmlparser::NAME));
    if (!name.empty())
    {
        atts.rtps.setName(name.c_str());
    }

    // server GuidPrefix is either pass as an attribute (preferred to allow profile reuse)
    // or inside the profile. 
    GuidPrefix_t & prefix = atts.rtps.prefix;
    const char * cprefix = server->Attribute(xmlparser::PREFIX);

    if (cprefix && !(std::istringstream(cprefix) >> prefix)
        && (prefix == c_GuidPrefix_Unknown))
    {
        LOG_ERROR("Servers cannot have a framework provided prefix"); // at least for now
        return;
    }

    GUID_t guid(prefix, c_EntityId_RTPSParticipant);

    // Check if the guidPrefix is already in use (there is a mistake on config file)
    if (_servers.find(guid) != _servers.end())
    {
        LOG_ERROR("DSManager detected two servers sharing the same prefix " << prefix);
        return;
    }

    // replace the atts.rtps.builtin lists with the ones from _server_locators (if present)
    // note that a previous call to DSManager::MapServerInfo
    serverLocator_map::mapped_type & lists = _server_locators[guid];
    if (!lists.first.empty() || !lists.second.empty())
    {   
        // server elements take precedence over profile ones
        // I copy them because other servers may need this values
        atts.rtps.builtin.metatrafficMulticastLocatorList = lists.first;
        atts.rtps.builtin.metatrafficUnicastLocatorList = lists.second;
    }

    // TODO: load the server list (if present) and update the atts.rtps.builtin
    tinyxml2::XMLElement *server_list = server->FirstChildElement(s_sSL.c_str());

    if (server_list)
    {
        RemoteServerList_t & list = atts.rtps.builtin.m_DiscoveryServers;
        list.clear(); // server elements take precedence over profile ones

        tinyxml2::XMLElement * rserver = server_list->FirstChildElement(s_sRServer.c_str());

        while (rserver)
        {
            RemoteServerList_t::value_type srv;
            GuidPrefix_t & prefix = srv.guidPrefix;

            // load the prefix
            const char * cprefix = rserver->Attribute(xmlparser::PREFIX);

            if (cprefix && !(std::istringstream(cprefix) >> prefix)
                && (prefix == c_GuidPrefix_Unknown))
            {
                LOG_ERROR("RServers must provide a prefix"); // at least for now
                return;
            }

            // load the locator lists
            serverLocator_map::mapped_type & lists = _server_locators[srv.GetParticipant()];
            srv.metatrafficMulticastLocatorList = lists.first;
            srv.metatrafficUnicastLocatorList = lists.second;

            list.push_back(std::move(srv));

            rserver = rserver->NextSiblingElement(s_sRServer.c_str());
        }
    }

    // We define the PDP as external (when moved to fast library it would be SERVER)
    BuiltinAttributes & b = atts.rtps.builtin;
    b.discoveryProtocol = EXTERNAL;
    b.m_PDPfactory.ReleasePDPInstance = &DSManager::ReleasePDPServer;

    // Choose the kind of server to create:
    if (server->BoolAttribute(s_sPersist.c_str()))
    {
        b.m_PDPfactory.CreatePDPInstance = &DSManager::createPDPServer<true>;  
    }
    else
    { 
        b.m_PDPfactory.CreatePDPInstance = &DSManager::createPDPServer<false>;
    }

    // now we create the new participant
    Participant * pServer = Domain::createParticipant(atts);

    if (!pServer)
    {
        LOG_ERROR("DSManager couldn't create the server " << prefix << " with profile " << profile_name);
        return;
    }
    
    addServer(pServer);
}


void DSManager::loadClient(tinyxml2::XMLElement* client)
{
    // clients are created for debugging purposes
    // profile name is mandatory because they must reference servers
    std::string profile_name(client->Attribute(xmlparser::PROFILE_NAME));

    if (profile_name.empty())
    {
        LOG_ERROR(xmlparser::PROFILE_NAME << " is a mandatory attribute of client tag");
        return;
    }

    // retrieve profile attributes
    ParticipantAttributes atts;
    if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::fillParticipantAttributes(profile_name, atts))
    {
        LOG_ERROR("DSManager::loadClient couldn't load profile " << profile_name);
        return;
    }

    // we must assert that PDPtype is CLIENT
    if (atts.rtps.builtin.discoveryProtocol != rtps::PDPType_t::CLIENT)
    {
        LOG_ERROR("DSManager::loadClient try to create a client with an incompatible profile: " << profile_name);
        return;
    }

    // pick the client's name (isn't mandatory to provide it). Takes precedence over profile provided.
    const char * name = client->Attribute(xmlparser::NAME);
    if( name != nullptr )
    {
        atts.rtps.setName(name);
    }

    // now we create the new participant
    Participant * pClient = Domain::createParticipant(atts);

    if (!pClient)
    {
        LOG_ERROR("DSManager couldn't create a client with profile " << profile_name);
        return;
    }

    addClient(pClient);
}

void DSManager::MapServerInfo(tinyxml2::XMLElement* server)
{
    uint8_t ident = 1;

    // profile name is mandatory
    std::string profile_name(server->Attribute(xmlparser::PROFILE_NAME));

    if (profile_name.empty())
    {
        LOG_ERROR(xmlparser::PROFILE_NAME << " is a mandatory attribute of server tag");
        return;
    }

    // server GuidPrefix is either pass as an attribute (preferred to allow profile reuse)
    // or inside the profile.
    GuidPrefix_t prefix;
    std::shared_ptr<ParticipantAttributes> atts;

    const char * cprefix = server->Attribute(xmlparser::PREFIX);

    if (cprefix)
    {
        std::istringstream(cprefix) >> prefix;
    }
    else
    {
        // I must load the prefix from the profile
        // retrieve profile attributes
        atts = std::make_shared<ParticipantAttributes>();
        if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::fillParticipantAttributes(profile_name, *atts))
        {
            LOG_ERROR("DSManager::loadServer couldn't load profile " << profile_name);
            return;
        }

        prefix = atts->rtps.prefix;
    }

    if (prefix == c_GuidPrefix_Unknown)
    {
        LOG_ERROR("Servers cannot have a framework provided prefix"); // at least for now
        return;
    }

    // Now we search the locator lists
    serverLocator_map::mapped_type pair;
    
    tinyxml2::XMLElement *LP = server->FirstChildElement(s_sLP.c_str());
    if (LP)
    {
        tinyxml2::XMLElement * list = LP->FirstChildElement(xmlparser::META_MULTI_LOC_LIST);

        if (list && (xmlparser::XMLP_ret::XML_OK != getXMLLocatorList(list, pair.first, ident)))
        {
            LOG_ERROR("Server " << prefix << " has an ill formed " << xmlparser::META_MULTI_LOC_LIST );
        }

        list = LP->FirstChildElement(xmlparser::META_UNI_LOC_LIST);
        if (list && (xmlparser::XMLP_ret::XML_OK != getXMLLocatorList(list, pair.second, ident)))
        {
            LOG_ERROR("Server " << prefix << " has an ill formed " << xmlparser::META_UNI_LOC_LIST);
        }

    }
    else
    {
        LocatorList_t multicast, unicast;

        // retrieve profile attributes
        if (!atts)
        {
            atts = std::make_shared<ParticipantAttributes>();
            if (xmlparser::XMLP_ret::XML_OK != xmlparser::XMLProfileManager::fillParticipantAttributes(profile_name, *atts))
            {
                LOG_ERROR("DSManager::loadServer couldn't load profile " << profile_name);
                return;
            }
        }

        pair.first = atts->rtps.builtin.metatrafficMulticastLocatorList;
        pair.second = atts->rtps.builtin.metatrafficUnicastLocatorList;
    }

    // now save the value
    _server_locators[GUID_t(prefix, c_EntityId_RTPSParticipant)] = std::move(pair);
    
}