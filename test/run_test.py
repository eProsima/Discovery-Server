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
"""Script to execute and validate a single Discovery Server v2 test."""
import argparse
import glob
import json
import logging
import os
import subprocess
import threading

import validation.CountLinesValidator as clv
import validation.GenerateValidator as genv
import validation.GroundTruthValidator as gtv
import validation.shared as shared


def parse_options():
    """
    Parse arguments.

    :return: The arguments parsed.
    """
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        add_help=True,
        description=(
            'Script to execute and validate a single Discovery Server v2 '
            'test.'),
        usage=(
            'python3 run_test.py'
            '-e <path/to/discovery-server/executable> -t <test-name>'
            '-T test_cases -g test_solutions')
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
        type=str,
        help='Name of the test case to execute (without xml extension).'
    )
    parser.add_argument(
        '-f',
        '--fds',
        type=str,
        help='Path to fast-discovery-server tool.'
    )
    parser.add_argument(
        '-d',
        '--debug',
        action='store_true',
        help='Print test execution.'
    )
    parser.add_argument(
        '-T',
        '--test-cases',
        type=str,
        required=True,
        help='Path to the directory containing the test cases.'
    )
    parser.add_argument(
        '-g',
        '--ground-truth',
        type=str,
        required=True,
        help='Path to the directory containing the expected snapshots.'
    )
    parser.add_argument(
        '-p',
        '--params',
        type=str,
        required=False,
        default=os.path.join('configuration', 'tests_params.json'),
        help='Path to the csv file which contains the tests parameters.'
    )

    return parser.parse_args()


def execute_test(
    test_params,
    discovery_server_tool_path,
    fds_path=None,
    debug=False
):
    """
    Run the Discovery Server v2 tests.

    :param test: The test to execute.
    :param path: The path of the xml configuration file of the test.
    :param discovery_server_tool_path: The path to the discovery server
        executable.
    :param fds_path: The path to the fast-discovery-server tool.
    :param debug: Debug flag (Default: False).
    """


    try:
        xml_config = test_params['xml_config_files']
    except KeyError:
        logger.error('XML configuration files not found')
        exit(1)

    if not xml_config:
        logger.error('XML configuration files not found')
        exit(1)

    events = []

    for xml in xml_config:
        if ('creation_time' in xml) and isinstance(xml['creation_time'], int):
            events.append(('RUN', xml['creation_time']))
        if ('kill_time' in xml) and isinstance(xml['creation_time'], int):
            events.append(('KILL', xml['kill_time']))

    events.sort(key=lambda x: x[1])

    # This could be a list comprehension
    for i, event in events:
        if i == 0:
            continue
        new_time = events[i-1][1] - event[1]
        events[i] = (event[0], new_time)

    exit(1)


    if test == 'test_16_lease_duration_single_client':
        aux_test_path = os.path.join(os.path.dirname(path), f'{test}_1.xml')
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

    elif test == 'test_17_lease_duration_remove_client_server':
        aux_test_path = os.path.join(os.path.dirname(path), f'{test}_1.xml')
        # Launch
        proc_main = subprocess.Popen(
            [discovery_server_tool_path, path])
        proc_sec = subprocess.Popen(
            [discovery_server_tool_path, aux_test_path])

        # Wait 5 seconds before killing the client server
        try:
            proc_sec.wait(15)
        except subprocess.TimeoutExpired:
            proc_sec.kill()

        # Wait for server completion
        proc_main.communicate()

    elif test == 'test_18_disposals_remote_servers_multiprocess':
        aux_test_path_1 = os.path.join(os.path.dirname(path), f'{test}_1.xml')
        aux_test_path_2 = os.path.join(os.path.dirname(path), f'{test}_2.xml')
        # Launch
        proc_main = subprocess.Popen(
            [discovery_server_tool_path, path])
        proc_sec_1 = subprocess.Popen(
            [discovery_server_tool_path, aux_test_path_1])
        proc_sec_2 = subprocess.Popen(
            [discovery_server_tool_path, aux_test_path_2])

        # Wait for completion
        proc_main.communicate()
        proc_sec_1.communicate()
        proc_sec_2.communicate()

    elif test == 'test_20_break_builtin_connections':
        aux_test_path = os.path.join(os.path.dirname(path), f'{test}_1.xml')
        # Launch
        proc_main = subprocess.Popen(
            [discovery_server_tool_path, path])
        proc_sec = subprocess.Popen(
            [discovery_server_tool_path, aux_test_path])

        # Wait 5 seconds before killing the server in the middle
        try:
            proc_sec.wait(20)
        except subprocess.TimeoutExpired:
            proc_sec.kill()

        # Wait for completion
        proc_main.communicate()

    elif test == 'test_23_fast_discovery_server_tool':
        if not fds_path:
            logger.error('Not provided path to fast-discovery-server tool.')
            exit(1)
        # Launch server and clients
        server = subprocess.Popen(
            [fds_path, '-i',  '1', '-l', '127.0.0.1', '-p', '23811'])
        clients = subprocess.Popen([discovery_server_tool_path, path])

        # Wait till clients test is finished, then kill the server
        clients.communicate()
        server.kill()

        if clients.returncode:
            logger.error(
                f'{test} process fault on clients: '
                f'returncode {clients.returncode}')
            exit(clients.returncode)

    elif test == 'test_24_backup':
        aux_test_path = os.path.join(os.path.dirname(path), f'{test}_1.xml')

        # Launch server and clients.
        proc_server = subprocess.Popen([discovery_server_tool_path, path])
        proc_client = subprocess.Popen(
            [discovery_server_tool_path, aux_test_path])

        # Wait 5 seconds before killing the server, time enought to have all
        # clients info recorded in the backup file
        try:
            proc_server.wait(5)
        except subprocess.TimeoutExpired:
            proc_server.kill()

        # Relaunch the server again and expect it reloads the backup data.
        result = subprocess.run([discovery_server_tool_path, path])

        if result.returncode:
            logger.error(f'Failure while running the backup server for {test}')
            logger.error(result.stdout)
            logger.error(result.stderr)
            exit(result.returncode)

        # Wait for client completion
        proc_client.communicate()

        if proc_client.returncode:
            logger.error(f'Failure when running clients for {test}')
            logger.error(result.stderr)
            exit(proc_client.returncode)

    elif test == 'test_26_backup_restore':
        aux_test_path = os.path.join(os.path.dirname(path), f'{test}_1.xml')
        # Launch and wait for completion
        proc_main = subprocess.run(
            [discovery_server_tool_path, path])
        if proc_main.returncode:
            logger.error(
                f'{test} process ha failed to launch the backup server: '
                f'returncode {proc_main.returncode}')
            exit(proc_main.returncode)

        proc_restore = subprocess.run(
            [discovery_server_tool_path, aux_test_path])

        if proc_restore.returncode:
            logger.error(
                f'{test} process has failed to restore the backup server: '
                f'returncode {proc_restore.returncode}')
            exit(proc_restore.returncode)

    else:
        subprocess.run([discovery_server_tool_path, path])


def run_and_validate(
    test_params,
    discovery_server_tool_path,
    fds_path=None,
    debug=False
):
    """
    Execute the tests and validate the output.

    :param test_params_df: The test parameters in a pandas Dataframe format.
    :param test_path: The path of the xml configuration file of the test.
    :param test_snapshot: Path to the snapshot xml file containing the
            Discovery-Server test output.
    :param ground_truth_snapshot: The path to the snapshot xml file containing
        the Discovery-Server ground-truth test output.
    :param discovery_server_tool: The path to the discovery server executable.
    :param fds_path: The path to the fast-discovery-server tool.
    :param debug: Debug flag (Default: False).

    :return: True if the test pass the validation, False otherwise.
    """
    test = test_params['id']
    logger.info('------------------------------------------------------------')
    logger.info(f"Running {test_params['id']}")
    execute_test(
        test_params, discovery_server_tool_path, fds_path, debug)
    logger.info('------------------------------------------------------------')

    validation_result = True

    validators = [
        clv.CountLinesValidator,
        genv.GenerateValidator,
        gtv.GroundTruthValidator]

    for i in range(1, test_params_df.iloc[0]['n_validations']+1):
        for validator in validators:
            val = validator(
                test_snapshot, ground_truth_snapshot, test_params_df)
            validation_result &= val.validate()

        test_snapshot = os.path.join(
            os.path.dirname(test_snapshot), f'{test}_{i}.snapshot~')
        ground_truth_snapshot = os.path.join(
            os.path.dirname(ground_truth_snapshot), f'{test}_{i}.snapshot')

    clear(test, validation_result, os.path.dirname(test_snapshot))

    return validation_result


def clear(test, validation, wd):
    """
    Clear the working directory removing the generated files.

    :param test: The test to execute.
    :param validation: The validation result.
    :param wd: The working directory where clear the generated files.
    """
    snapshots = glob.glob(
        os.path.join(wd, f'{test}*.snapshot~'))

    # Only remove the generated snapshots for valid tests, keeping the failing
    # test results for debuging purposes.
    if validation:
        for s in snapshots:
            try:
                os.remove(s)
            except OSError:
                logger.error(f'Error while deleting file: {s}')

    db_files = glob.glob(
        os.path.join(wd, '*.json'))
    db_files.extend(glob.glob(
        os.path.join(wd, '*.db')))

    for f in db_files:
        try:
            os.remove(f)
        except OSError:
            logger.error(f'Error while deleting file: {f}')


def load_test_params(test_name, tests_params_path):
    """
    Load test parameters.

    :param test_name: The unique identifier of the test.
    :param tests_params_path: The path to the json file which contains the
        test parameters.

    :return: The Python dict resulting from reading the test params.
    """
    if os.path.isfile(tests_params_path):
        with open(tests_params_path) as json_file:
            tests_params = json.load(json_file)

        try:
            test_params = tests_params['tests'][test_name]
        except KeyError:
            logger.error(f'Not supported test: {test_name}')
            exit(1)

    else:
        logger.error('Not found file with test parameters.')
        exit(1)

    return test_params


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

    if run_and_validate(
        load_test_params(args.test, args.params),
        args.exe,
        fds_path=(args.fds if args.fds else None),
        debug=args.debug
    ):
        logger.info(
            f'Overall test result for {args.test}: '
            f'{shared.bcolors.OK}PASS{shared.bcolors.ENDC}')
        exit(0)
    else:
        logger.info(
            f'Overall test result for {args.test}: '
            f'{shared.bcolors.FAIL}FAIL{shared.bcolors.ENDC}')
        exit(1)
