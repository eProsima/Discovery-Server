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
Script implementing the CountLinesValidator class.

The CountLinesValidator validates the test counting and comparing the number
of lines of the output snapshot resulted from the test execution and an a
priori well known output.
"""
import shared.shared as shared

import validation.Validator as validator

class CountLinesValidator(validator.Validator):
    """
    Class to validate an snapshot resulting from a Discovery-Server test.

    Validate the test counting and comparing the number of lines of the
    output snapshot resulted from the test execution and an a priori well
    known output.
    """

    def _validator_tag(self):
        """Return validator's tag in json parameters file."""
        return 'count_lines_validation'

    def _validate(self):
        """Validate the test counting the number of lines."""

        lines_get = 0
        lines_expected = 0

        try:
            file_name = self.validation_params_['file_path']
            output_file_name = self.validator_input_.result_file

            self.logger.debug(f'Snapshot to validate: {file_name}')
            self.logger.debug(f'Output snapshot: {output_file_name}')

            with open(file_name) as f:
                lines_get = len(f.readlines())
            with open(output_file_name) as f:
                lines_expected = len(f.readlines())
        except (IOError, ValueError) as e:
            self.logger.error(e)
            return shared.ReturnCode.ERROR

        self.logger.debug(f'CountLinesValidator: Lines in result snapshot:'
                          f'{lines_get}, expected lines in snapshot:'
                          f'{lines_expected}')

        val = (lines_get == lines_expected)

        return shared.ReturnCode.OK if val else shared.ReturnCode.FAIL
