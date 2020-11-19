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
import validation.Validator as validator
import validation.shared as shared


class CountLinesValidation(validator.Validator):
    """
    Class to validate an snapshot resulting from a Discovery-Server test.

    Validate the test counting and comparing the number of lines of the
    output snapshot resulted from the test execution and an a priori well
    knonw output.
    """

    def virtual_validate(self):
        """Validate the test counting the number of lines."""
        lines_get = 0

        try:
            with open(self.snapshot_file_path) as f:
                for line in f.readlines():
                    lines_get += 1

            lines_expected = 0
            with open(self.gt_snapshot_file_path) as f:
                for line in f.readlines():
                    lines_expected += 1
        except IOError as e:
            self.logger.error(e)
            return shared.ReturnCode.ERROR

        val = lines_get == lines_expected

        return shared.ReturnCode.OK if val else shared.ReturnCode.FAIL
