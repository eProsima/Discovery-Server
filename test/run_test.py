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

import validation.CountLinesValidation as clv
import validation.GenerateValidation as genv
import validation.GroundTruthValidation as gtv
import validation.TestParams as params
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
        command = [discovery_server_tool_path, path]
        if debug:
            subprocess.call(command)
        else:
            subprocess.call(command, stdout=subprocess.DEVNULL)


def count_lines_check(
    test,
    test_snapshot_path,
    ground_truth_snapshot_path,
    test_params
):
    """
    Validate the test counting counting the number of lines.

    The output snapshot resulting from the test execution is compared with an
    a priori well knonw output.

    :param test_snapshot: The path to the test output.
    :param ground_truth_snapshot: The path to the a priori calculated test
        output.
    """
    os.path.join(test_snapshot_path, f'{args.test}.snapshot~'),
    os.path.join(ground_truth_snapshot_path, f'{args.test}.snapshot'),

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
    not_supported_tests = [
        'test_18_disposals_remote_servers_multiprocess',
        'test_20_break_builtin_connections',
        'test_23_fast_discovery_server_tool',
        'test_24_backup',
        'test_26_backup_restore']

    if test in not_supported_tests:
        logger.warning(
            f'Not supported validation of {test}: '
            f'{shared.bcolors.WARNING}SKIP{shared.bcolors.ENDC}')
        return True

    disposals_tests = [
        'test_13_disposals_single_server',
        'test_16_lease_duration_single_client']

    server_endpoints_tests = [
        'test_07_server_endpoints_two_servers',
        'test_08_server_endpoints_four_clients',
        'test_11_remote_servers',
        'test_12_virtual_topics',
        'test_14_disposals_remote_servers',
        'test_15_disposals_client_servers',
        'test_17_lease_duration_remove_client_server',
        'test_19_disposals_break_builtin_connections',
        'test_25_backup_compatibility',
        'test_27_tcp']

    val = genv.GenerateValidation(
        test_snapshot,
        disposals=(test in disposals_tests),
        server_endpoints=(test in server_endpoints_tests))

    return val.validate()


def validate_test(
    test,
    test_path,
    test_snapshot,
    ground_truth_snapshot,
    discovery_server_tool_path,
    test_params,
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
    logger.info('------------------------------------------------------------')
    logger.info(f'Running {test}')
    execute_test(
        test, test_path, discovery_server_tool_path, fds_path, debug)
    logger.info('------------------------------------------------------------')

    lines_count_ret = count_lines_check(
        test, test_snapshot, ground_truth_snapshot, test_params)

    gt_ret = ground_truth_check(test_snapshot, ground_truth_snapshot)

    gen_ret = generate_check(test, test_snapshot)

    validation_result = lines_count_ret and gt_ret and gen_ret

    # Backup test needs to validate 2 snapshots, the snapshot from the server
    # process and the snapshot from the clients process.
    if test == 'test_24_backup' or test == 'test_26_backup_restore':
        aux_test_snapshot = os.path.join(
            os.path.dirname(test_snapshot), f'{test}_1.snapshot~')
        aux_ground_truth_snapshot = os.path.join(
            os.path.dirname(ground_truth_snapshot), f'{test}_1.snapshot')

        lines_count_ret = count_lines_check(
            aux_test_snapshot, aux_ground_truth_snapshot)

        gt_ret = ground_truth_check(
            aux_test_snapshot, aux_ground_truth_snapshot)

        gen_ret = generate_check(test, aux_test_snapshot)

        validation_result = validation_result and (
            validation_result and lines_count_ret and gt_ret and gen_ret)

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


def supported_test(test):
    """Define supported tests."""
    tests = [
        'test_01_trivial',
        'test_02_single_server_medium',
        'test_03_single_server_large',
        'test_04_server_ping',
        'test_05_server_double_ping',
        'test_06_diamond_servers',
        'test_07_server_endpoints_two_servers',
        'test_08_server_endpoints_four_clients',
        'test_09_servers_serial',
        'test_10_server_redundancy',
        'test_11_remote_servers',
        'test_12_virtual_topics',
        'test_13_disposals_single_server',
        'test_14_disposals_remote_servers',
        'test_15_disposals_client_servers',
        'test_16_lease_duration_single_client',
        'test_17_lease_duration_remove_client_server',
        'test_18_disposals_remote_servers_multiprocess',
        'test_19_disposals_break_builtin_connections',
        'test_20_break_builtin_connections',
        'test_21_disposals_remote_server_trivial',
        'test_22_environment_variable_setup',
        'test_23_fast_discovery_server_tool',
        'test_24_backup',
        'test_25_backup_compatibility',
        'test_26_backup_restore',
        'test_27_tcp']

    return test in tests


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

    if not supported_test(args.test):
        logger.error(f'Not supported test: {args.test}')
        exit(1)

    # Load Test Params
    test_params = params.TestParams(
        os.path.join(args.test_cases, f'tests_params.csv'))
    if not test_params.is_loaded():
        logger.error(f'Error reading test parameters')
        exit(1)

    if validate_test(
        args.test,
        os.path.join(args.test_cases, f'{args.test}.xml'),
        os.path.join(os.getcwd(), f'{args.test}.snapshot~'),
        os.path.join(args.ground_truth, f'{args.test}.snapshot'),
        args.exe,
        test_params,
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
