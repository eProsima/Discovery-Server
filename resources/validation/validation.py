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
"""Script to validate an snapshot resulting from a Discovery-Server test."""
import itertools
import json
import logging
from pathlib import Path

import jsondiff

import shared

import xmltodict


class Validation(object):
    """Class to validate an snapshot resulting from a Discovery-Server test."""

    def __init__(
        self,
        snapshot,
        debug=False,
        logger=None
    ):
        """
        Build a validation object.

        Constructor of the validation class.

        :param snapshot: Path to the snapshot xml file containing the
            Discovery-Server test output.
        :param logger: The logging object. VALIDATION if None logger is
            provided.
        :param debug: True/False to activate/deactivate debug logger.
        """
        self.set_logger(logger, debug)
        self.logger.debug('Creating an instance of {}'.format(type(self)))

        self.parse_xml_snapshot(snapshot)
        self.copy_dict = {'DS_Snapshots': {}}
        self.validate_dict = {'DS_Snapshots': {}}
        self.process_servers()

    def validate(self):
        """Validate the snapshot resulting from a Discovery-Server test."""
        try:
            self.__create_copy_and_validate_dict()
            self.__process_validation_dict()
        except KeyError as e:
            logging.debug(e)

        if self.__dict_equal(self.copy_dict, self.validate_dict):
            return True

        return False

    def save_generated_json_files(
        self,
        parsed_json=False,
        copy_json=False,
        validate_json=False,
        parsed_json_file='parsed_snapshot.json',
        copy_json_file='copy_snapshot.json',
        validate_json_file='validation_snapshot.json'
    ):
        """Save the generated dictionaries in json files."""
        if parsed_json:
            self.__write_json_file(self.snapshot_dict, parsed_json_file)
        if copy_json:
            self.__write_json_file(self.copy_dict, copy_json_file)
        if validate_json:
            self.__write_json_file(self.validate_dict, validate_json_file)

    def parse_xml_snapshot(
        self,
        xml_file_path
    ):
        """
        Read an xml snapshot file and convert it into a dictionary.

        :param xml_file_path: The path to the xml snapshot.
        """
        if shared.get_file_extension(xml_file_path) != '.snapshot':
            raise ValueError(
                f'The snapshot file \"{xml_file_path}\" '
                'is not an .snapshot file')
        xml_file_path = Path(xml_file_path).resolve()
        valid_path, xml_file = shared.is_valid_path(xml_file_path)
        if not valid_path:
            raise ValueError(f'NOT valid snapshot path: {xml_file}')

        with open(xml_file_path) as xml_file:
            self.snapshot_dict = xmltodict.parse(xml_file.read())

        return self.snapshot_dict

    def set_logger(self, logger, debug):
        """
        Instance the class logger.

        :param logger: The logging object. VALIDATION if None logger is
            provided.
        :param debug: True/False to activate/deactivate debug logger.
        """
        if isinstance(logger, logging.Logger):
            self.logger = logger
        else:
            l_handler = logging.StreamHandler()
            l_format = '[%(asctime)s][%(name)s][%(levelname)s] %(message)s'
            l_format = logging.Formatter(l_format)
            l_handler.setFormatter(l_format)

            if isinstance(logger, str):
                self.logger = logging.getLogger(logger)
                self.logger.addHandler(l_handler)
            else:
                self.logger = logging.getLogger('VALIDATION')
                self.logger.addHandler(l_handler)

        if debug:
            self.logger.setLevel(logging.DEBUG)
        else:
            self.logger.setLevel(logging.INFO)

    def process_servers(self):
        """Generate a list with the servers guid_prefix from the snapshot."""
        self.servers = []
        try:
            for snapshot in self.__dict2list(
                    self.snapshot_dict['DS_Snapshots']['DS_Snapshot']):
                for ptdb in self.__dict2list(snapshot['ptdb']):
                    [self.servers.append(ptdi['@guid_prefix'])
                        for ptdi in self.__dict2list(ptdb['ptdi'])
                        if (ptdi['@server'] == 'true' and
                            ptdi['@guid_prefix'] not in self.servers)]
        except KeyError as e:
            logging.debug(e)

        return self.servers

    def __create_copy_and_validate_dict(self):
        """
        Create the copy and validation dicts parsing the original snapshot.

        The copy dictionary contains the relevant elements of the original
        snapshot dictionary. Each element of this original dictionary is parsed
        to be unique in the new copy dictionary. On the other hand, the basic
        structure of the validation dictionary is created. This basic structure
        is the participants (ptdi) and endpoints (publisher/subscriber)
        instances of a local participant (ptdb).
        These elements are not dependent on the other participants and can
        therefore be filled in the first parse of the original snapshot
        dictionary.
        """
        for snapshot in self.__dict2list(
                self.snapshot_dict['DS_Snapshots']['DS_Snapshot']):
            self.copy_dict[
                'DS_Snapshots'][
                f"DS_Snapshot_{snapshot['@timestamp']}"] = {}
            self.validate_dict[
                'DS_Snapshots'][
                f"DS_Snapshot_{snapshot['@timestamp']}"] = {}
            for ptdb in self.__dict2list(snapshot['ptdb']):
                if ptdb['@guid_prefix'] in self.servers:
                    continue

                self.copy_dict[
                    'DS_Snapshots'][
                    f"DS_Snapshot_{snapshot['@timestamp']}"][
                    f"ptdb_{ptdb['@guid_prefix']}"] = {
                        'guid_prefix': ptdb['@guid_prefix']}
                self.validate_dict[
                    'DS_Snapshots'][
                    f"DS_Snapshot_{snapshot['@timestamp']}"][
                    f"ptdb_{ptdb['@guid_prefix']}"] = {
                        'guid_prefix': ptdb['@guid_prefix']}

                for ptdi in self.__dict2list(ptdb['ptdi']):
                    if ptdi['@server'] == 'true':
                        continue

                    self.copy_dict[
                        'DS_Snapshots'][
                        f"DS_Snapshot_{snapshot['@timestamp']}"][
                        f"ptdb_{ptdb['@guid_prefix']}"][
                        f"ptdi_{ptdi['@guid_prefix']}"] = {
                            'guid_prefix': ptdi['@guid_prefix']}

                    if (ptdi['@guid_prefix'] == ptdb['@guid_prefix']):
                        self.validate_dict[
                            'DS_Snapshots'][
                            f"DS_Snapshot_{snapshot['@timestamp']}"][
                            f"ptdb_{ptdb['@guid_prefix']}"][
                            f"ptdi_{ptdi['@guid_prefix']}"] = {
                                'guid_prefix': ptdi['@guid_prefix']}

                    if 'publisher' in (x.lower() for x in ptdi.keys()):
                        for pub in self.__dict2list(ptdi['publisher']):
                            publisher_guid = '{}.{}'.format(
                                pub['@guid_prefix'], pub['@guid_entity'])
                            self.copy_dict[
                                'DS_Snapshots'][
                                f"DS_Snapshot_{snapshot['@timestamp']}"][
                                f"ptdb_{ptdb['@guid_prefix']}"][
                                f"ptdi_{ptdi['@guid_prefix']}"][
                                f'publisher_{publisher_guid}'] = {
                                    'topic': pub['@topic'],
                                    'guid': publisher_guid
                                }
                            if (ptdi['@guid_prefix'] == ptdb['@guid_prefix']):
                                self.validate_dict[
                                    'DS_Snapshots'][
                                    f"DS_Snapshot_{snapshot['@timestamp']}"][
                                    f"ptdb_{ptdb['@guid_prefix']}"][
                                    f"ptdi_{ptdi['@guid_prefix']}"][
                                    f'publisher_{publisher_guid}'] = {
                                        'topic': pub['@topic'],
                                        'guid': publisher_guid
                                    }

                    if 'subscriber' in (x.lower() for x in ptdi.keys()):
                        for sub in self.__dict2list(ptdi['subscriber']):
                            subscriber_guid = '{}.{}'.format(
                                sub['@guid_prefix'], sub['@guid_entity'])
                            self.copy_dict[
                                'DS_Snapshots'][
                                f"DS_Snapshot_{snapshot['@timestamp']}"][
                                f"ptdb_{ptdb['@guid_prefix']}"][
                                f"ptdi_{ptdi['@guid_prefix']}"][
                                f'subscriber_{subscriber_guid}'] = {
                                    'topic': sub['@topic'],
                                    'guid': subscriber_guid
                                }
                            if (ptdi['@guid_prefix'] == ptdb['@guid_prefix']):
                                self.validate_dict[
                                    'DS_Snapshots'][
                                    f"DS_Snapshot_{snapshot['@timestamp']}"][
                                    f"ptdb_{ptdb['@guid_prefix']}"][
                                    f"ptdi_{ptdi['@guid_prefix']}"][
                                    f'subscriber_{subscriber_guid}'] = {
                                        'topic': sub['@topic'],
                                        'guid': subscriber_guid
                                    }

    def __process_validation_dict(self):
        """Set the validation dictionary with the remote known participants.

        This function iterates over the validation dictionary to map and set
        known remote participants along with their publishers and subscribers.
        """
        for snapshot in self.__dict2list(
                self.snapshot_dict['DS_Snapshots']['DS_Snapshot']):
            for ptdb in self.__dict2list(snapshot['ptdb']):
                for ptdi in self.__dict2list(ptdb['ptdi']):
                    if ptdi['@server'] == 'true':
                        continue

                    if 'publisher' in (x.lower() for x in ptdi.keys()):
                        for pub in self.__dict2list(ptdi['publisher']):
                            self.__fill_matching_publisher_data(
                                    ptdi['@guid_prefix'],
                                    pub['@guid_entity'],
                                    pub['@topic'])

                    if 'subscriber' in (x.lower() for x in ptdi.keys()):
                        for sub in self.__dict2list(ptdi['subscriber']):
                            self.__fill_matching_subscriber_data(
                                    ptdi['@guid_prefix'],
                                    sub['@guid_entity'],
                                    sub['@topic'])

    def __fill_matching_publisher_data(
        self,
        ptdi_guid_prefix,
        publisher_guid_entity,
        topic
    ):
        """
        Set the publisher data which match the subscriber topic.

        Search for participants with subscribers on the same topic as the
        publisher and add the remote publisher's data to the local
        participant.

        :param ptdi_guid_prefix: The remote participant guid prefix.
        :param publisher_guid_entity: The remote publisher guid entity.
        :param topic: The publisher topic.
        """
        publisher_guid = f'{ptdi_guid_prefix}.{publisher_guid_entity}'

        for snapshot in self.__dict2list(
                self.snapshot_dict['DS_Snapshots']['DS_Snapshot']):
            gen_ptdb = (
                ptdb for ptdb in self.__dict2list(snapshot['ptdb'])
                if ptdi_guid_prefix != ptdb['@guid_prefix'] and
                ptdb['@guid_prefix'] not in self.servers)
            for ptdb in gen_ptdb:
                gen_ptdi = (
                    ptdi for ptdi in self.__dict2list(ptdb['ptdi'])
                    if 'subscriber' in (x.lower() for x in ptdi.keys()))
                gen_ptdi = self.__has_next(gen_ptdi)
                if gen_ptdi is not None:
                    for sub in self.__dict2list(next(gen_ptdi)['subscriber']):
                        if topic == sub['@topic']:
                            v_ptdb = self.validate_dict[
                                    'DS_Snapshots'][
                                    ('DS_Snapshot_'
                                        f"{snapshot['@timestamp']}")][
                                    f"ptdb_{ptdb['@guid_prefix']}"]
                            if (f'ptdi_{ptdi_guid_prefix}'
                                    not in v_ptdb.keys()):
                                self.validate_dict[
                                    'DS_Snapshots'][
                                    ('DS_Snapshot_'
                                        f"{snapshot['@timestamp']}")][
                                    f"ptdb_{ptdb['@guid_prefix']}"][
                                    f'ptdi_{ptdi_guid_prefix}'] = {
                                        'guid_prefix': ptdi_guid_prefix
                                    }

                            self.validate_dict[
                                'DS_Snapshots'][
                                f"DS_Snapshot_{snapshot['@timestamp']}"][
                                f"ptdb_{ptdb['@guid_prefix']}"][
                                f'ptdi_{ptdi_guid_prefix}'][
                                f'publisher_{publisher_guid}'] = {
                                        'topic': topic,
                                        'guid': publisher_guid
                                }

    def __fill_matching_subscriber_data(
        self,
        ptdi_guid_prefix,
        subscriber_guid_entity,
        topic
    ):
        """
        Set the subscribers data which match the publisher topic.

        Search for participants with publishers on the same topic as the
        subscriber and add the remote subscriber's data to the local
        participant.

        :param ptdi_guid_prefix: The remote participant guid prefix.
        :param subscriber_guid_entity: The remote subscriber guid entity.
        :param topic: The subscriber topic.
        """
        subscriber_guid = f'{ptdi_guid_prefix}.{subscriber_guid_entity}'

        for snapshot in self.__dict2list(
                self.snapshot_dict['DS_Snapshots']['DS_Snapshot']):
            gen_ptdb = (
                ptdb for ptdb in self.__dict2list(snapshot['ptdb'])
                if ptdi_guid_prefix != ptdb['@guid_prefix'] and
                ptdb['@guid_prefix'] not in self.servers)
            for ptdb in gen_ptdb:
                gen_ptdi = (
                    ptdi for ptdi in self.__dict2list(ptdb['ptdi'])
                    if 'publisher' in (x.lower() for x in ptdi.keys()))
                gen_ptdi = self.__has_next(gen_ptdi)
                if gen_ptdi is not None:
                    for pub in self.__dict2list(next(gen_ptdi)['publisher']):
                        if topic == pub['@topic']:
                            v_ptdb = self.validate_dict[
                                    'DS_Snapshots'][
                                    f"DS_Snapshot_{snapshot['@timestamp']}"][
                                    f"ptdb_{ptdb['@guid_prefix']}"]
                            if (f'ptdi_{ptdi_guid_prefix}'
                                    not in v_ptdb.keys()):
                                self.validate_dict[
                                    'DS_Snapshots'][
                                    f"DS_Snapshot_{snapshot['@timestamp']}"][
                                    f"ptdb_{ptdb['@guid_prefix']}"][
                                    f'ptdi_{ptdi_guid_prefix}'] = {
                                        'guid_prefix': ptdi_guid_prefix
                                    }

                            self.validate_dict[
                                'DS_Snapshots'][
                                f"DS_Snapshot_{snapshot['@timestamp']}"][
                                f"ptdb_{ptdb['@guid_prefix']}"][
                                f'ptdi_{ptdi_guid_prefix}'][
                                f'subscriber_{subscriber_guid}'] = {
                                        'topic': topic,
                                        'guid': subscriber_guid
                                }

    def __dict2list(self, d):
        """
        Cast an item from a dictionary to a list if it is not already one.

        :param d: The dictionary item.
        :return: The list with the dictionary items as elements of the list.
        """
        return d if isinstance(d, list) else [d]

    def __write_json_file(
        self,
        data_dict,
        json_file_path
    ):
        """
        Write a dictionary in a json file.

        :param data_dict: The dictionary object.
        :param json_file_path: The path to the json file.
        """
        data_dict_str = json.dumps(data_dict, indent=4)
        with open(json_file_path, 'w') as cp_file:
            cp_file.write(data_dict_str)

    def __dict_equal(self, dict_a, dict_b):
        """
        Check if true dictionaries are equal.

        :param dict_a: The first disctionary.
        :param dict_b: The second disctionary.
        :return: True if dict_a is equal to dict_b.
        """
        json_a = json.dumps(dict_a, sort_keys=True)
        json_b = json.dumps(dict_b, sort_keys=True)

        return not jsondiff.diff(json_a, json_b)

    def __has_next(self, iterable):
        try:
            content = next(iterable)
        except StopIteration:
            return None
        return itertools.chain([content], iterable)
