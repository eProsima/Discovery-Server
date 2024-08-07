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
"""
Script implementing the GroundTruthValidator class.

The GroundTruthValidator validates the Discovery-Server test by comparing
the test output with a previously generated snapshot containing the exact
output that the test output should have if it passes.
"""
import json

import jsondiff

import shared.shared as shared

import validation.Validator as validator


class GroundTruthValidator(validator.Validator):
    """
    Class to validate an snapshot resulting from a Discovery-Server test.

    Validate the Discovery-Server test by comparing the test output with a
    previously generated snapshot containing the exact output that the test
    output should have if it passes.
    """

    def _validator_tag(self):
        """Return validator's tag in json parameters file."""
        return 'ground_truth_validation'

    def _validate(self):
        """Validate the snapshots resulting from a Discovery-Server test."""
        # Get parameters from test params
        try:
            self.guidless = self.validation_params_['guidless']
            self.val_snapshot = \
                self.parse_xml_snapshot(self.validation_params_['file_path'])
            self.gt_snapshot = \
                self.parse_xml_snapshot(self.validator_input_.result_file)

        except (KeyError, ValueError) as e:
            self.logger.error(e)
            self.logger.error('Incorrect arguments in validator'
                              'parametrization')
            return shared.ReturnCode.ERROR

        self.gt_dict = {'DS_Snapshots': {}}
        self.val_dict = {'DS_Snapshots': {}}
        self.servers = self.process_servers()

        if self.guidless:
            self.logger.debug('Groundtruth validator in GuidLess mode.')
            self.__trim_snapshot_dict_guidless(self.gt_snapshot, self.gt_dict)
            self.__trim_snapshot_dict_guidless(self.val_snapshot, self.val_dict)
        else:
            self.__trim_snapshot_dict(self.gt_snapshot, self.gt_dict)
            self.__trim_snapshot_dict(self.val_snapshot, self.val_dict)

        n_tests = 0
        successful_tests = []
        failed_tests = []
        error_tests = []

        for snapshot in self.gt_dict['DS_Snapshots']:
            n_tests += 1
            try:
                if self.__dict_equal(
                        self.gt_dict['DS_Snapshots'][snapshot],
                        self.val_dict['DS_Snapshots'][snapshot]):
                    self.logger.debug(
                        f'Validation result of Snapshot {snapshot}: '
                        f'{shared.bcolors.OK}PASS{shared.bcolors.ENDC}')
                    successful_tests.append(snapshot)
                else:
                    self.logger.debug(
                        f'Validation result of Snapshot {snapshot}: '
                        f'{shared.bcolors.FAIL}FAIL{shared.bcolors.ENDC}')
                    failed_tests.append(snapshot)

            except KeyError as e:
                self.logger.error(e)
                self.logger.error(
                        f'Validation result of Snapshot {snapshot}: '
                        f'{shared.bcolors.FAIL}FAIL{shared.bcolors.ENDC}')
                error_tests.append(snapshot)

        if error_tests:
            return shared.ReturnCode.ERROR
        elif failed_tests:
            return shared.ReturnCode.FAIL
        else:
            return shared.ReturnCode.OK

    def save_generated_json_files(
        self,
        validate_json=False,
        ground_truth_json=False,
        validate_json_file='input_snapshot.json',
        ground_truth_json_file='ground_truth_snapshot.json'
    ):
        """Save the generated dictionaries in json files."""
        if validate_json:
            self.__write_json_file(self.validate_dict, validate_json_file)
        if ground_truth_json:
            self.__write_json_file(self.copy_dict, ground_truth_json_file)

    def process_servers(self):
        """Generate a list with the servers guid_prefix from the snapshot."""
        servers = []
        try:
            for snapshot in self.__dict2list(
                    self.gt_snapshot['DS_Snapshots']['DS_Snapshot']):
                for ptdb in self.__dict2list(snapshot['ptdb']):
                    [servers.append(ptdi['@guid_prefix'])
                        for ptdi in self.__dict2list(ptdb['ptdi'])
                        if (ptdi['@server'] == 'true' and
                            ptdi['@guid_prefix'] not in servers)]
        except KeyError as e:
            self.logger.debug(e)

        return servers

    def __trim_snapshot_dict(self, original_dict, trimmed_dict):
        """
        Create the ground truth and validation dicts parsing the snapshots.

        :param original_dict: The original dictionary.
        :param trimmed_dict: The resulting dictionary after trim.
        """
        for ds_snapshot in self.__dict2list(
                original_dict['DS_Snapshots']['DS_Snapshot']):
            trimmed_dict['DS_Snapshots'][f"{ds_snapshot['description']}"] = {}

            try:
                ptdb_l = self.__dict2list(ds_snapshot['ptdb'])
            except KeyError:
                self.logger.debug(
                    f"Snapshot {ds_snapshot['@timestamp']} does not "
                    'contain any participant.')
                continue

            for ptdb in ptdb_l:
                trimmed_dict[
                    'DS_Snapshots'][
                    f"{ds_snapshot['description']}"][
                    f"ptdb_{ptdb['@guid_prefix']}"] = {
                        'guid_prefix': ptdb['@guid_prefix']}

                try:
                    ptdi_l = self.__dict2list(ptdb['ptdi'])
                except KeyError:
                    self.logger.debug(
                        f"Participant {ptdb['@guid_prefix']} does not "
                        'match any remote participant.')
                    continue

                for ptdi in ptdi_l:
                    trimmed_dict[
                        'DS_Snapshots'][
                        f"{ds_snapshot['description']}"][
                        f"ptdb_{ptdb['@guid_prefix']}"][
                        f"ptdi_{ptdi['@guid_prefix']}"] = {
                            'guid_prefix': ptdi['@guid_prefix']}

                    if 'publisher' in (x.lower() for x in ptdi.keys()):
                        for pub in self.__dict2list(ptdi['publisher']):
                            publisher_guid = '{}.{}'.format(
                                pub['@guid_prefix'], pub['@guid_entity'])
                            trimmed_dict[
                                'DS_Snapshots'][
                                f"{ds_snapshot['description']}"][
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
                            trimmed_dict[
                                'DS_Snapshots'][
                                f"{ds_snapshot['description']}"][
                                f"ptdb_{ptdb['@guid_prefix']}"][
                                f"ptdi_{ptdi['@guid_prefix']}"][
                                f'subscriber_{subscriber_guid}'] = {
                                    'topic': sub['@topic'],
                                    'guid': subscriber_guid
                                }

    def __trim_snapshot_dict_guidless(self, original_dict, trimmed_dict):
        """
        Create the ground truth and validation dicts parsing the snapshots in guidless mode.
        The @name parameter is used instead of the guid to uniquely identify the entities.

        :param original_dict: The original dictionary.
        :param trimmed_dict: The resulting dictionary after trim.
        """
        for ds_snapshot in self.__dict2list(
                original_dict['DS_Snapshots']['DS_Snapshot']):
            trimmed_dict['DS_Snapshots'][f"{ds_snapshot['description']}"] = {}

            try:
                ptdb_l = self.__dict2list(ds_snapshot['ptdb'])
            except KeyError:
                self.logger.debug(
                    f"Snapshot {ds_snapshot['@timestamp']} does not "
                    'contain any participant.')
                continue

            for ptdb in ptdb_l:
                trimmed_dict[
                    'DS_Snapshots'][
                    f"{ds_snapshot['description']}"][
                    f"ptdb_{ptdb['@name']}"] = {
                        'name': ptdb['@name']}

                try:
                    ptdi_l = self.__dict2list(ptdb['ptdi'])
                except KeyError:
                    self.logger.debug(
                        f"Participant {ptdb['@name']} does not "
                        'match any remote participant.')
                    continue

                for ptdi in ptdi_l:
                    if ptdi['@name'] == '':
                        ptdi_name = ptdb['@name']
                    else:
                        ptdi_name = ptdi['@name']

                    trimmed_dict[
                        'DS_Snapshots'][
                        f"{ds_snapshot['description']}"][
                        f"ptdb_{ptdb['@name']}"][
                        f"ptdi_{ptdi_name}"] = {
                            'name': ptdi_name}

                    # Publishers and subscribers do not have names, but are in this ptdi block,
                    # so we can assume that they belong to this participant.
                    if 'publisher' in (x.lower() for x in ptdi.keys()):
                        for pub in self.__dict2list(ptdi['publisher']):
                            publisher_id = '{}_{}'.format(
                                ptdi_name, pub['@guid_entity'])
                            assert (ptdi['@guid_prefix'] == pub['@guid_prefix'])
                            trimmed_dict[
                                'DS_Snapshots'][
                                f"{ds_snapshot['description']}"][
                                f"ptdb_{ptdb['@name']}"][
                                f"ptdi_{ptdi_name}"][
                                f'publisher_{publisher_id}'] = {
                                    'topic': pub['@topic']
                                }

                    if 'subscriber' in (x.lower() for x in ptdi.keys()):
                        for sub in self.__dict2list(ptdi['subscriber']):
                            subscriber_id = '{}_{}'.format(
                                ptdi_name, sub['@guid_entity'])
                            assert (ptdi['@guid_prefix'] == sub['@guid_prefix'])
                            trimmed_dict[
                                'DS_Snapshots'][
                                f"{ds_snapshot['description']}"][
                                f"ptdb_{ptdb['@name']}"][
                                f"ptdi_{ptdi_name}"][
                                f'subscriber_{subscriber_id}'] = {
                                    'topic': sub['@topic']
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
