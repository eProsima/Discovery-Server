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
import argparse
import logging
import os
import subprocess

import validation.CountLinesValidation as clv
import validation.GenerateValidation as genv
import validation.GroundTruthValidation as gtv
import validation.shared as shared


def parse_options():
    """
    Parse arguments.

    :return: The arguments parsed.
    """
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        description="""
        Script to execute every test in 'test_cases' subfolder and check it
        is working correctly."""
    )
    parser.add_argument(
        '-e',
        '--exe',
        type=str,
        required=True,
        help='Path to discovery-server executable.'
    )
    parser.add_argument(
        '-t',
        '--test',
        nargs='*',
        type=str,
        help=(
            'List of names of test case to execute (only name without xml).'
            'No set to test all.')
    )
    parser.add_argument(
        '-d',
        '--debug',
        action='store_true',
        help='Print test execution.'
    )
    parser.add_argument(
        '-s',
        '--snapshot',
        type=str,
        help='Path to the xml file containing the test result snapshot.'
    )
    parser.add_argument(
        '-g',
        '--ground-truth',
        type=str,
        help='Path to the xml file containing the ground-truth snapshot.'
    )
    parser.add_argument(
        '--save-dicts',
        action='store_true',
        help='Save the generated dictionaries in json files.'
    )
    parser.add_argument(
        '-c',
        '--input-json',
        default=os.path.join(os.path.curdir, './input_snapshot.json'),
        required=False,
        help='Path to the generated json file containing the snapshot data.'
    )
    parser.add_argument(
        '-v',
        '--ground-truth-json',
        default=os.path.join(os.path.curdir, './ground_truth_snapshot.json'),
        required=False,
        help='Path to the validation json file containing the snapshot data.'
    )

    return parser.parse_args()


def path_to_test(test=None):
    """
    Create a list of tuples (test name, path to test).

    :param test: List or name of the test to include in the list of tuples.
        If None, all test are included.
    """
    p_dir_path = os.getcwd()
    test_cases_path = os.path.join(p_dir_path, 'test_cases')

    tests = [f for f in os.listdir(test_cases_path) if (
        os.path.isfile(os.path.join(test_cases_path, f))
        and not f.startswith('_'))]

    # Prevent to try to run a test that does not exist
    tests = [
        ('.'.join(t.split('.')[:-1]), os.path.join(test_cases_path, t))
        for t in tests]

    if test is not None:
        tests = [f for f in tests if f[0] in test]

    return tests


def execute_test(test, path, discovery_server_tool_path, debug=False):
    """
    Run the Discovery Server v2 tests.

    :param test: The test to execute.
    :param path: The path of the xml configuration file of the test.
    :param discovery_server_tool: The path to the discovery server executable.
    :param debug: Debug flag (Default: False).
    """
    if 'lease_duration' in test:
        aux_test_path = (
            f"{'/'.join(path.split('/')[:-1])}/_{path.split('/')[-1]}")
        if not debug:
            # Launch
            proc_server = subprocess.Popen(
                [discovery_server_tool_path, path],
                stdout=subprocess.DEVNULL)
            proc_client = subprocess.Popen(
                [discovery_server_tool_path, aux_test_path],
                stdout=subprocess.DEVNULL)
        else:
            # Launch
            proc_server = subprocess.Popen(
                [discovery_server_tool_path, path])
            proc_client = subprocess.Popen(
                [discovery_server_tool_path, aux_test_path])

        # Wait 5 seconds before killing the external client
        try:
            proc_client.wait(5)
        except subprocess.TimeoutExpired:
            proc_client.kill()

        # Wait for server completion
        proc_server.communicate()
    else:
        command = [discovery_server_tool_path, path]

        if not debug:
            subprocess.call(command, stdout=subprocess.DEVNULL)
        else:
            subprocess.call(command)


def get_result_and_expected_paths(test):
    """
    Get the paths of the test output and the expected output.

    :param test: The name of the test to validate.
    """
    result_file_path = os.path.join(os.getcwd(), f'{test}.snapshot~')
    expected_file_path = os.path.join(
        os.getcwd(), 'test_solutions', f'{test}.snapshot')

    return result_file_path, expected_file_path


def count_lines_check(test_snapshot, ground_truth_snapshot):
    """
    Validate the test counting counting the number of lines.

    The output snapshot resulting from the test execution is compared with an
    a priori well knonw output.

    :param test_snapshot: The path to the test output.
    :param ground_truth_snapshot: The path to the a priori calculated test
        output.
    """
    val = clv.CountLinesValidation(test_snapshot, ground_truth_snapshot)

    return val.validate()


def ground_truth_check(test_snapshot, ground_truth_snapshot):
    """
    Validate the test snapshot output with a ground-truth snapshot.

    The output snapshot resulting from the test execution is compared with an
    a priori well knonw output.

    :param test_snapshot: The path to the test output.
    :param ground_truth_snapshot: The path to the a priori calculated test
        output.
    """
    val = gtv.GroundTruthValidation(test_snapshot, ground_truth_snapshot)

    return val.validate()


def generate_check(test, test_snapshot):
    """
    Validate the test snapshot output with a generated snapshot.

    The output snapshot resulting from the test execution is validated by
    generating the expected output. The generated output is built by matching
    the clients endpoints.

    :param test_snapshot: The path to the test output.
    """
    server_endpoints_tests = [
        'test_own_endpoints_multiple_servers_trivial',
        'test_own_endpoints_multiple_servers',
        'test_own_endpoints',
        'test_virtual_topics_large',
        'test_virtual_topics_medium']

    disposals_tests = ['test_disposals_edp']

    val = genv.GenerateValidation(
        test_snapshot,
        disposals=(test in disposals_tests),
        server_endpoints=(test in server_endpoints_tests))

    return val.validate()


def validate_test(test, test_path, discovery_server_tool_path, debug=False):
    """
    Execute the tests and validate the output.

    :param test: The test to execute.
    :param path: The path of the xml configuration file of the test.
    :param discovery_server_tool: The path to the discovery server executable.
    :param debug: Debug flag (Default: False).
    """
    logger.info('------------------------------------------------------------')
    logger.info(f'Running {test}')
    execute_test(test, test_path, discovery_server_tool_path, debug)
    logger.info('------------------------------------------------------------')

    test_snapshot, ground_truth_snapshot = get_result_and_expected_paths(test)

    lines_count_ret = count_lines_check(test_snapshot, ground_truth_snapshot)
    gt_ret = ground_truth_check(test_snapshot, ground_truth_snapshot)

    gen_ret = generate_check(test, test_snapshot)

    os.system('rm ' + test_snapshot)

    return lines_count_ret and gt_ret and gen_ret


if __name__ == '__main__':

    # Parse arguments
    args = parse_options()

    # Create a custom logger
    logger = logging.getLogger('VALIDATION')
    # Create handlers
    l_handler = logging.StreamHandler()
    # Create formatters and add it to handlers
    l_format = '[%(asctime)s][%(name)s][%(levelname)s] %(message)s'
    l_format = logging.Formatter(l_format)
    l_handler.setFormatter(l_format)
    # Add handlers to the logger
    logger.addHandler(l_handler)
    # Set log level
    if args.debug:
        logger.setLevel(logging.DEBUG)
    else:
        logger.setLevel(logging.INFO)

    tests = path_to_test(args.test)

    for test, test_path in tests:
        if validate_test(test, test_path, args.exe, debug=args.debug):
            logger.info(
                f'Overall test result for {test}: '
                f'{shared.bcolors.OK}PASS{shared.bcolors.ENDC}')
        else:
            logger.info(
                f'Overall test result for {test}: '
                f'{shared.bcolors.FAIL}FAIL{shared.bcolors.ENDC}')
