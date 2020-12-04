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
"""Script implementing the Validator class."""
import logging
import os
from pathlib import Path

import validation.shared as shared

import xmltodict


def validation_file_path(file_path):
    """Std file path to validate an snapshot."""
    return os.path.join(file_path, '~')


class Validator(object):
    """
    Class to validate an snapshot resulting from a Discovery-Server test.

    This is the base class of various classes that implement a different type
    of test output validation. Each of the child classes will implement its
    validation mechanism in the _validate() function.
    """

    def __init__(
        self,
        snapshot_file_path,
        ground_truth_snapshot_file_path,
        test_params,
        debug=False,
        logger=None
    ):
        """
        Build a generic validation object.

        :param snapshot_file_path: The path to the snapshot xml file
            containing the Discovery-Server test output.
        :param ground_truth_snapshot_file_path: The path to the snapshot xml
            file containing the Discovery-Server ground-truth test output.
        :param test_params: The test parameters in a pandas Dataframe format.
        :param debug: True/False to activate/deactivate debug logger.
        :param logger: The logging object. VALIDATION if None
            logger is provided.
        """
        self.set_logger(logger, debug)
        self.logger.debug(f'Creating an instance of {self.__validator_name()}')

        self.snapshot_file_path = self.valid_snapshot_path(snapshot_file_path)
        self.gt_snapshot_file_path = self.valid_snapshot_path(
            ground_truth_snapshot_file_path)

    def set_logger(self, logger, debug):
        """
        Instance the class logger.

        :param logger: The logging object. VALIDATION if None
            logger is provided.
        :param debug: True/False to activate/deactivate debug logger.
        """
        if isinstance(logger, logging.Logger):
            self.logger = logger
        else:
            if isinstance(logger, str):
                self.logger = logging.getLogger(logger)
            else:
                self.logger = logging.getLogger('VALIDATION')

            if not self.logger.hasHandlers():
                l_handler = logging.StreamHandler()
                l_format = '[%(asctime)s][%(name)s][%(levelname)s] %(message)s'
                l_format = logging.Formatter(l_format)
                l_handler.setFormatter(l_format)
                self.logger.addHandler(l_handler)

        if debug:
            self.logger.setLevel(logging.DEBUG)
        else:
            self.logger.setLevel(logging.INFO)

    def valid_snapshot_path(
        self,
        xml_file_path
    ):
        """
        Read an xml snapshot file and convert it into a dictionary.

        :param xml_file_path: The path to the xml snapshot.
        """
        if (shared.get_file_extension(xml_file_path)
                not in ['.snapshot', '.snapshot~']):
            raise ValueError(
                f'The snapshot file \"{xml_file_path}\" '
                'is not an .snapshot file')
        xml_file_path = Path(xml_file_path).resolve()
        valid_path, xml_file = shared.is_valid_path(xml_file_path)
        if not valid_path:
            raise ValueError(f'NOT valid snapshot path: {xml_file}')

        return xml_file_path

    def parse_xml_snapshot(
        self,
        xml_file_path
    ):
        """
        Read an xml snapshot file and convert it into a dictionary.

        :param xml_file_path: The path to the xml snapshot.
        """
        if (shared.get_file_extension(xml_file_path)
                not in ['.snapshot', '.snapshot~']):
            raise ValueError(
                f'The snapshot file \"{xml_file_path}\" '
                'is not an .snapshot file')
        xml_file_path = Path(xml_file_path).resolve()
        valid_path, xml_file = shared.is_valid_path(xml_file_path)
        if not valid_path:
            raise ValueError(f'NOT valid snapshot path: {xml_file}')

        with open(xml_file_path) as xml_file:
            snapshot_dict = xmltodict.parse(xml_file.read())

        return snapshot_dict

    def validate(self):
        """Validate the test counting the number of lines."""
        res = self._validate()

        if res == shared.ReturnCode.OK:
            self.logger.info(
                    f'Result of {self.__validator_name()}: '
                    f'{shared.bcolors.OK}{res.name}{shared.bcolors.ENDC}')

            return True

        elif res == shared.ReturnCode.SKIP:
            self.logger.warning(
                    f'Result of {self.__validator_name()}: '
                    f'{shared.bcolors.WARNING}{res.name}{shared.bcolors.ENDC}')

            return True

        else:
            self.logger.error(
                    f'Result of {self.__validator_name()}: '
                    f'{shared.bcolors.FAIL}{res.name}{shared.bcolors.ENDC}')

        return False

    def _validate(self):
        """
        Implement the specific validation.

        Depending on the validator, this method implements
        the different validation possibilities.
        """
        pass

    def __validator_name(self):
        """Return validator's name."""
        return type(self).__name__
