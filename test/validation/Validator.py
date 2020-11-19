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
"""Script to execute Discovery Server v2 tests."""
import logging
import os
from pathlib import Path

import validation.shared as shared


def validation_file_path(file_path):
    """Std file path to validate an snapshot."""
    return os.path.join(file_path, '~')


class Validator(object):
    """
    Class to validate an snapshot resulting from a Discovery-Server test.

    Validate the test counting and comparing the number of lines of the
    output snapshot resulted from the test execution and an a priori well
    knonw output.
    """

    def __init__(
        self,
        snapshot_file_path,
        test_params,
        debug=False,
        logger=None
    ):
        """
        Build a generic validation object.

        :param snapshot_file_path: The path to the test output.
        :param disposals: True if it is a disposals test (Default: False).
        :param server_endpoints: True if there are endpoints in the servers
            (Default: False).
        :param logger: The logging object. COUNT_LINES_VALIDATION if None
            logger is provided.
        :param debug: True/False to activate/deactivate debug logger.
        """
        self.set_logger(logger, debug)
        self.logger.debug('Creating an instance of {}'.format(type(self)))

        self.snapshot_file_path = self.valid_snapshot_path(snapshot_file_path)
        self.test_params = test_params

    def set_logger(self, logger, debug):
        """
        Instance the class logger.

        :param logger: The logging object. COUNT_LINES_VALIDATION if None
            logger is provided.
        :param debug: True/False to activate/deactivate debug logger.
        """
        if isinstance(logger, logging.Logger):
            self.logger = logger
        else:
            if isinstance(logger, str):
                self.logger = logging.getLogger(logger)
            else:
                self.logger = logging.getLogger('COUNT_LINES_VALIDATION')

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

    def validate(self):
        """Validate the test counting the number of lines."""
        res = self.virtual_validate()

        color = shared.bcolors.ENDC
        if res == shared.error_code.OK:
            color = shared.bcolors.OK
        elif res == shared.error_code.SKIP:
            color = shared.bcolors.OK
        elif res == shared.error_code.ERROR:
            color = shared.bcolors.ERROR
        elif res == shared.error_code.FAIL:
            color = shared.bcolors.WARNING

        self.logger.info(
                f'Result of {self.validator_name()}: '
                f'{color}{res.name}{shared.bcolors.ENDC}')

        return res

    def virtual_validate(self):
        """
        Implement the specific validation.

        Depending on the validator, this method implements
        the different validation possibilities.
        """
        pass

    def validator_name(self):
        """Return validator's name."""
        pass
