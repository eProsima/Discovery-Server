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
import json

import jsondiff

import validation.Validator as validator
import validation.shared as shared


class GroundTruthValidation(validator.Validator):
    """Class to validate an snapshot resulting from a Discovery-Server test."""

    def __init__(
        self,
        snapshot_file_path,
        ground_truth_snapshot_file_path,
        test_params,
        debug=False,
        logger=None
    ):
        """
        Build a validation object.

        Constructor of the ground-truth validation class.

        :param snapshot: Path to the snapshot xml file containing the
            Discovery-Server test output.
        :param snapshot: Path to the snapshot xml file containing the
            Discovery-Server ground-truth test output.
        :param logger: The logging object. GROUND_TRUTH_VALIDATION if None
            logger is provided.
        :param debug: True/False to activate/deactivate debug logger.
        """
        super().__init__(
            snapshot_file_path,
            ground_truth_snapshot_file_path,
            test_params,
            debug,
            logger
        )

        self.gt_dict = {'DS_Snapshots': {}}
        self.val_dict = {'DS_Snapshots': {}}
        self.process_servers()

    def virtual_validate(self):
        """Validate the snapshots resulting from a Discovery-Server test."""
        self.__trim_snapshot_dict(self.gt_snapshot, self.gt_dict)
        self.__trim_snapshot_dict(self.val_snapshot, self.val_dict)

        n_tests = 0
        successful_tests = []
        failed_tests = []
        error_tests = []

        for snapshot in self.gt_dict['DS_Snapshots']:
            n_tests += 1
            val = False
            try:
                val = self.__dict_equal(
                    self.gt_dict['DS_Snapshots'][snapshot],
                    self.val_dict['DS_Snapshots'][snapshot])

                if val:
                    self.logger.info(
                        f'Validation result of Snapshot {snapshot}: '
                        f'{shared.bcolors.OK}PASS{shared.bcolors.ENDC}')
                    successful_tests.append(snapshot)
                else:
                    self.logger.info(
                        f'Validation result of Snapshot {snapshot}: '
                        f'{shared.bcolors.FAIL}FAIL{shared.bcolors.ENDC}')
                    failed_tests.append(snapshot)

            except KeyError as e:
                self.logger.error(e)
                self.logger.info(
                        f'Validation result of Snapshot {snapshot}: '
                        f'{shared.bcolors.FAIL}FAIL{shared.bcolors.ENDC}')
                error_tests.append(snapshot)

        self.logger.info(
            f'Summary: {n_tests} tests, {len(error_tests)} errors, '
            f'{len(failed_tests)} failures')

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
        self.servers = []
        try:
            for snapshot in self.__dict2list(
                    self.gt_snapshot['DS_Snapshots']['DS_Snapshot']):
                for ptdb in self.__dict2list(snapshot['ptdb']):
                    [self.servers.append(ptdi['@guid_prefix'])
                        for ptdi in self.__dict2list(ptdb['ptdi'])
                        if (ptdi['@server'] == 'true' and
                            ptdi['@guid_prefix'] not in self.servers)]
        except KeyError as e:
            self.logger.debug(e)

        return self.servers

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
