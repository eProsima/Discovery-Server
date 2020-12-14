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

import validation.CountLinesValidator as clv
import validation.ExitCodeValidation as ecv
import validation.GenerateValidator as genv
import validation.GroundTruthValidator as gtv
import validation.shared as shared


def get_validators():
    """Get a list of validators implemented."""
    return [
        ecv.ExitCodeValidation,
        clv.CountLinesValidator,
        # genv.GenerateValidator,
        # gtv.GroundTruthValidator,
    ]


def validate_test(
    process_id,
    validation_params,
    process_execution,
    debug=False,
    logger=None
):
    """
    Validate thread execution.
    """

    validators = get_validators()
    final_result = True

    for validator_constructor in validators:
        validator = validator_constructor(
            process_execution,
            validation_params,
            debug)

        result = validator.validate()
        print_result(
            logger,
            f'Result of test {process_id} for validator {validator.name()}',
            result
        )

        if result != shared.ReturnCode.OK and result != shared.ReturnCode.SKIP:
            final_result = False

    return final_result


def print_result(logger, msg, res):
    """Print test result with specific color depending result."""
    if (type(res) is bool):
        print_result_bool(logger, msg, res)

    color = shared.bcolors.FAIL
    log_func = logger.info

    if res == shared.ReturnCode.SKIP:
        color = shared.bcolors.WARNING
        log_func = logger.debug
    elif res == shared.ReturnCode.OK:
        color = shared.bcolors.OK
    elif res == shared.ReturnCode.ERROR:
        log_func = logger.error

    log_func(
        f'{msg}: '
        f'{color}{res.name}{shared.bcolors.ENDC}')


def print_result_bool(logger, msg, res):
    """Print test result with specific color depending boolean result."""
    if res:
        color = shared.bcolors.OK
        logger.info(
            f'{msg}: '
            f'{color}PASS{shared.bcolors.ENDC}')
    else:
        color = shared.bcolors.FAIL
        logger.info(
            f'{msg}: '
            f'{color}FAIL{shared.bcolors.ENDC}')
