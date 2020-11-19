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
"""Script to execute a single Discovery Server v2 test."""
import argparse
import glob
import logging
import os
import subprocess

import pandas as pd

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
        type=str,
        help=('Name of the test case to execute (without xml extension.')
    )
    parser.add_argument(
        '-f',
        '--fds',
        type=str,
        help=('Path to fast-discovery-server tool.')
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
        help='Path to the directory containin the test cases.'
    )
    parser.add_argument(
        '-g',
        '--ground-truth',
        type=str,
        required=True,
        help='Path to the directory containin the expected snapshots.'
    )

    return parser.parse_args()


def execute_test(
    test,
    path,
    discovery_server_tool_path,
    fds_path=None,
    debug=False
):
    """
    Run the Discovery Server v2 tests.

    :param test: The test to execute.
    :param path: The path of the xml configuration file of the test.
    :param discovery_server_tool: The path to the discovery server executable.
    :param debug: Debug flag (Default: False).
    """
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


def validate_test(
    test_params_df,
    test_path,
    test_snapshot,
    ground_truth_snapshot,
    discovery_server_tool_path,
    fds_path=None,
    debug=False
):
    """
    Execute the tests and validate the output.

    :param test: The test to execute.
    :param path: The path of the xml configuration file of the test.
    :param discovery_server_tool: The path to the discovery server executable.
    :param debug: Debug flag (Default: False).
    """
    test = test_params_df.iloc[0]['test_name']
    logger.info('------------------------------------------------------------')
    logger.info(f'Running {test}')
    execute_test(
        test, test_path, discovery_server_tool_path, fds_path, debug)
    logger.info('------------------------------------------------------------')

    validation_result = True

    validators = [
        clv.CountLinesValidation,
        genv.GenerateValidation,
        gtv.GroundTruthValidation]

    for i in range(1, test_params_df.iloc[0]['n_validations']+1):
        for v in validators:
            val = v(test_snapshot, ground_truth_snapshot, test_params_df)
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


def load_test_params(test_name):
    """
    Load test parameters.

    :param test_name: The unique identifier of the test.
    """
    tests_params_path = os.path.join(os.getcwd(), 'test', 'tests_params.csv')
    if os.path.isfile(tests_params_path):
        tests_params_df = pd.read_csv(tests_params_path)
        test_params_df = tests_params_df.loc[
            tests_params_df['test_name'] == test_name]

        if test_params_df.empty:
            logger.error(f'Not supported test: {test_name}')
            exit(1)

    else:
        logger.error('Not found file with test parameters.')
        exit(1)

    return test_params_df


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

    # Load test parameters
    test_params_df = load_test_params(args.test)

    if validate_test(
        test_params_df,
        os.path.join(args.test_cases, f'{args.test}.xml'),
        os.path.join(os.getcwd(), f'{args.test}.snapshot~'),
        os.path.join(args.ground_truth, f'{args.test}.snapshot'),
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
