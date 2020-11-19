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
from pathlib import Path

import validation.shared as shared


class CountLinesValidation(object):
    """
    Class to validate an snapshot resulting from a Discovery-Server test.

    Validate the test counting and comparing the number of lines of the
    output snapshot resulted from the test execution and an a priori well
    knonw output.
    """

    def __init__(
        self,
        result_file_path,
        expected_file_path,
        debug=False,
        logger=None
    ):
        """
        Build a validation object.

        Constructor of the count lines validation class.

        :param result_file_path: The path to the test output.
        :param expected_file_path: The path to the a priori calculated test
            output.
        :param logger: The logging object. COUNT_LINES_VALIDATION if None
            logger is provided.
        :param debug: True/False to activate/deactivate debug logger.
        """
        self.set_logger(logger, debug)
        self.logger.debug('Creating an instance of {}'.format(type(self)))

        self.result_file_path = self.valid_snapshot_path(result_file_path)
        self.expected_file_path = self.valid_snapshot_path(expected_file_path)

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
        lines_get = 0

        try:
            with open(self.result_file_path) as f:
                for line in f.readlines():
                    lines_get += 1

            lines_expected = 0
            with open(self.expected_file_path) as f:
                for line in f.readlines():
                    lines_expected += 1
        except IOError:
            return shared.error_code.FAIL

        val = lines_get == lines_expected

        if val:
            self.logger.info(
                'Result of counting lines validation: '
                f'{shared.bcolors.OK}PASS{shared.bcolors.ENDC}')
            return shared.error_code.OK
        else:
            self.logger.info(
                'Result of counting lines validation: '
                f'{shared.bcolors.FAIL}FAIL{shared.bcolors.ENDC}')
            return shared.error_code.ERROR
