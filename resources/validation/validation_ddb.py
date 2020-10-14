
# Copyright 2020 Proyectos y Sistemas de Mantenimiento SL (eProsima).
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Script to validate a json resulting from a DiscoveryDataBase dump."""

import json

# check if the ddb state is correct
def validate_state(state, debug=False, run_all=False):
    v = Validation(state, debug=debug, run_all=run_all)
    if debug:
        print(v)
    res = v.validate()
    if not res:
        print ("ERRORS")
        for e in v.errors:
            print(" " + e)
    return res


class Validation:

    def __init__(self, state, run_all=False, debug=False):
        self.state_ = state
        self.run_all_ = run_all
        self.debug_ = debug
        self.functions_list_ = [
            self.check_is_server_participant,
            self.check_is_server_acked_by_all,
            self.check_every_entity_acked_by_server_and_writer,
            self.check_entities_in_participants,
            self.check_entities_in_its_own_participants,
            self.check_not_release_valid_change,
            self.check_topics,
            self.check_endpoints_match,
            self.check_relevant_list,
            self.check_to_dispose,
            self.check_disposed
        ]
        self.errors = []
        self._initialize_null()

    def _initialize_null(self):
        return Validation._initialize_dic_null(self.state_)        

    def validate(self):
        res = True
        for f in self.functions_list_:
            if not f():
                print ("FAIL - " + str(f.__name__))
                if self.run_all_:
                    res = False
                else:
                    return False
            elif self.debug_:
                print ("CORRECT - " + str(f.__name__))
        return res


    def __str__ (self):
        return json.dumps(self.state_, indent=4)


    ############################
    # auxiliar functions

    # return the guid of the own server
    def server_guid(self):
        return self.state_["server_guid"]

    # return a dictionary of the participants
    def participants(self):
        return self.state_["participants"]

    # return a dict of the alive participants in the state
    def participants_alive(self):
        p_dic = {}
        for p, info in self.participants().items():
            if Validation.change_is_alive(info["change"]):
                p_dic[p] = info
        return p_dic

    # return all the entities
    def entities(self):
        ent = self.participants()
        ent.update(self.readers())
        ent.update(self.writers())
        return ent

    # return a dict of the alive entities in the state
    def entities_alive(self):
        p_dic = {}
        for p, info in self.entities().items():
            if Validation.change_is_alive(info["change"]):
                p_dic[p] = info
        return p_dic

    # return the dict of the writers
    def writers(self):
        return self.state_["writers"]

    # return the dict of the readers
    def readers(self):
        return self.state_["readers"]

    # return the dict of writers by topic
    def writers_by_topic(self):
        return self.state_["writers_by_topic"]

    # return the dict of readers by topic
    def readers_by_topic(self):
        return self.state_["readers_by_topic"]

    # return the relevant participant map for the <participant>
    def relevant_participants_map(self, participant):
        if participant not in self.participants().keys():
            return {} # ERROR
        return self.participants()[participant]["relevant_participants_map"]

    # return true if <relevant_participant_guid> is relevant to <participant_guid>
    def is_relevant(self, participant_guid, relevant_participant_guid):
        if participant_guid not in self.participants():
            return False
        elif relevant_participant_guid not in self.relevant_participants_map(participant_guid).keys():
            return False
        return True

    # return true if <participant_guid> has been acked by <relevant_participant_guid>
    def ack_status(self, participant_guid, relevant_participant_guid):
        if participant_guid not in self.participants():
            return False
        elif relevant_participant_guid not in self.relevant_participants_map(participant_guid).keys():
            return False 
        return self.relevant_participants_map(participant_guid)[relevant_participant_guid]

    ############################
    # check functions       
    
    # it exist a participant that is this server
    def check_is_server_participant(self):
        if not self.server_guid() in self.participants().keys():
            self.errors.append("Server guid does not exist in Participants")
            return False
        return True
        
    # the server is acked by all value only true when every participant knows it
    def check_is_server_acked_by_all(self):
        server_guid = self.server_guid()
        acked_by_all = True
        for p in self.participants_alive().keys():
            if not self.ack_status(server_guid, p):
                acked_by_all = False
                break
        if not acked_by_all == self.state_["server_acked_by_all"]:
            return False
        return True

    # every entity must be acked by the server and its writer
    def check_every_entity_acked_by_server_and_writer(self):
        server_guid = self.server_guid()
        for p, info in self.entities_alive().items():
            if not (self.ack_status(p, server_guid)):
                self.errors.append("Participant " + p + " has not server as acked")
                return False

            if not self.ack_status(p, Validation.getGuidPrefix(Validation.change_writer(info["change"]))):
                self.errors.append("Participant <" + p + "> has not its writer <" + 
                    Validation.getGuidPrefix(Validation.change_writer(info["change"])) + "> as acked")
                return False
        return True

    # every endpoint must be in its participant
    def check_entities_in_participants(self):

        # check all writers
        for endpoint_guid in self.writers():
            endpoint_prefix = Validation.getGuidPrefix(endpoint_guid)
            if endpoint_prefix not in self.participants_alive().keys():
                return False
            else:
                if endpoint_guid not in self.participants()[endpoint_prefix]["writers"].values():
                    return False
        
        # check all readers
        for endpoint_guid in self.readers():
            endpoint_prefix = Validation.getGuidPrefix(endpoint_guid)
            if endpoint_prefix not in self.participants_alive().keys():
                return False
            else:
                if endpoint_guid not in self.participants()[endpoint_prefix]["readers"].values():
                    return False
                    
        return True


    # every entity in a participant must have its GuidPrefix and be in endpoints list
    def check_entities_in_its_own_participants(self):
        
        # for every participant
        for participant_guid, participant_info in self.participants().items():

            # for every endpoint writer
            for writer_guid in participant_info["writers"].values():
                if Validation.getGuidPrefix(writer_guid) != participant_guid:
                    return False
                if writer_guid not in self.writers().keys():
                    return False

            # for every endpoint reader
            for reader_guid in participant_info["readers"].values():
                if Validation.getGuidPrefix(reader_guid) != participant_guid:
                    return False
                if reader_guid not in self.readers().keys():
                    return False

        return True

    
    # every change to release must not be the change in any entity
    def check_not_release_valid_change(self):
        for change in self.state_["changes_to_release"]:
            for entity, info in self.entities().items():
                if Validation.change_is_equal(change, info["change"]):
                    return False
        return True

    # every endpoint (ALIVE) must be in its wr_by_topic
    def check_topics(self):
        return True # TODO

    # check every endpoint that must be matched is matched or pending to match
    # check also that dirty topics are those that need to be updated again
    def check_endpoints_match(self):
        return True # TODO

    # every entity has between its relevants only those who need to be
    def check_relevant_list(self):
        return True # TODO

    # every change to send in disposals must be NOT ALIVE
    def check_to_dispose(self):
        return True # TODO

    # check disposed entities are correctly connected and erased
    def check_disposed(self):
        return True # TODO


    ############################
    # static auxiliar functions

    # compare two dictionaries of change
    def change_is_equal(change1, change2):
        return change1 == change2

    # return true if the change is ALIVE kind
    def change_is_alive(change):
        return change["kind"] == 0

    # return true if the change is ALIVE kind
    def change_writer(change):
        return Validation.getGuidPrefix(change["writer_guid"])

    # return the Prefix of a guid
    def getGuidPrefix(guid):
        if guid == "|GUID UNKNOWN|":
            return "0"
        return guid.split("|")[0]

    # return true if the guid is from a participant
    def isParticipant(guid):
        # be aware that the string of 0x00 is 0
        return "".join(guid.split("|")[1]) == "001c1"  

    # return true if the guid is from a reader
    def isReader(guid):
        return guid.split("|")[1].split(".")[0] in ["2", "3", "c2", "c3"]

    # return true if the guid is from a writer
    def isWriter(guid):
        return guid.split("|")[1].split(".")[0] in ["4", "7", "c4", "c7"]

    # # return the dictionary of this name in the state or an empty one if it does not exist
    # def get_vector(state, name):
    #     if name in state.keys():
    #         return state[name]
    #     else:
    #         return {}

    def _initialize_dic_null(dic):
        for v, i in dic.items():
            if i is None:
                dic[v] = {}
            elif isinstance(i, dict):
                Validation._initialize_dic_null(i)      

    def dic_subset(dic1, dic2):
        for k, v in dic1.items():
            if k not in dic2.keys():
                return False
            if isinstance(v, dict):
                if not isinstance(dic2[k], dict):
                    return False
                else:
                    if not dic_subset(v, dic2[k]):
                        return False
            else:
                if v != dic2[k]:
                    return False
        return True