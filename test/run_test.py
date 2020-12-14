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
import functools
import glob
import json
import logging
import os
import subprocess
import threading
import time

import validation.shared as shared
import validation.validation as val

DESCRIPTION = """Script to execute and validate Discovery Server v2 test"""
USAGE = ('python3 run_test.py -e <path/to/discovery-server/executable>'
         ' -p <path/to/test/parameteres/file> [-t LIST[test-name]]'
         ' [-f <path/to/fastd)ds/tool>] [-d] [-r] [-i <bool>] [-s <bool>]')
MAX_TIME = 60*5


def parse_options():
    """
    Parse arguments.

    :return: The arguments parsed.
    """
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        add_help=True,
        description=(DESCRIPTION),
        usage=(USAGE)
    )
    required_args = parser.add_argument_group('required arguments')
    required_args.add_argument(
        '-e',
        '--exe',
        type=str,
        required=True,
        help='Path to discovery-server executable.'
    )
    parser.add_argument(
        '-p',
        '--params',
        type=str,
        required=False,
        default=os.path.join('configuration', 'tests_params.json'),
        help='Path to the csv file which contains the tests parameters.'
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
        required=False,
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
        '-r',
        '--not-remove',
        action='store_true',
        help='Do not remove generated files.'
    )
    parser.add_argument(
        '-s',
        '--shm',
        type=str,
        required=False,
        help='Use shared memory transport. Default both.'
    )
    parser.add_argument(
        '-i',
        '--intraprocess',
        type=str,
        required=False,
        help='Use intraprocess transport. Default both.'
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


def execute_validate_thread_test(
    test_id,
    result_list,
    ds_tool_path,
    process_params,
    config_file,
    flags,
    fds_path=None,
    debug=False
):
    """
    Execute a single process inside a test and validate it.
    """
    # PARAMENTER CONFIGURATION
    # Get process id
    try:
        process_id = process_params['process_id']
    except KeyError:
        logger.error(f'Missing <process_id> in process of test {process_id}')
        result_list.append(False)
        return False

    logger.debug(f'Getting paramenters for test {test_id} '
                 f'in process: {process_id}')

    # Get creation time (default 0)
    creation_time = process_params.get('creation_time', 0)

    # Get kill time (default MaxTime)
    kill_time = process_params.get('kill_time', MAX_TIME)

    # Get  environment variables
    try:
        env_var = [(env['name'], env['value']) for env in
                   process_params['environment_variables']]
    except KeyError:
        env_var = []

    xml_config_file = None
    # Check whether it must execute the DS tool or the fastdds tool
    if 'xml_config_file' in process_params.keys():
        xml_config_file = process_params['xml_config_file']

    elif 'tool_config' in process_params.keys():
        server_id = 0
        server_address = None
        server_port = None
        if fds_path is None:
            logger.error(f'Non tool given and needed')
            result_list.append(False)
            return False

        try:
            server_id = process_params['tool_config']['id']
            server_address = process_params['tool_config']['address']
            server_port = process_params['tool_config']['port']
        except KeyError:
            pass
    else:
        logger.error(f'Incorrect process paramenters: {test_id}-{process_id}')
        result_list.append(False)
        return False

    # EXECUTION
    # Wait for initial time
    time.sleep(creation_time)

    # Set env var
    my_env = os.environ.copy()
    for name, value in env_var:
        logger.debug(f'Adding envvar {name}:{value} to {test_id}-{process_id}')
        my_env[name] = value
    # Set configuration file
    my_env['FASTRTPS_DEFAULT_PROFILES_FILE'] = config_file
    logger.debug(f'Configuration file {config_file} to {test_id}-{process_id}')

    # Launch
    if xml_config_file is not None:
        # Configuration file
        process_args = [ds_tool_path, xml_config_file] + flags

    else:
        # Fastdds tool
        process_args = [fds_path, '-i', str(server_id)]
        if server_address is not None:
            process_args.append('-l')
            process_args.append(str(server_address))
        if server_port is not None:
            process_args.append('-p')
            process_args.append(str(server_port))

    # Execute
    logger.debug(f'Executing process {process_id} in test {test_id} with '
                 f'command {process_args}')
    proc = subprocess.Popen(process_args, env=my_env)

    # Wait 5 seconds before killing the external client
    try:
        proc.wait(kill_time)
    except subprocess.TimeoutExpired:
        proc.kill()

    # Wait for server completion
    proc.communicate()

    # VALIDATION
    if 'validation' not in process_params.keys():
        result_list.append(True)
        return True

    logger.debug(f'Executing validation for process {process_id} '
                 f'in test {test_id}')
    validation_params = process_params['validation']

    process_name = test_id + '.process_' + str(process_id)

    result = val.validate_test(
        process_name,
        validation_params,
        proc,
        debug,
        logger
    )
    result_list.append(result)
    return result


def execute_validate_test(
    test_name,
    test_id,
    ds_tool_path,
    test_params,
    config_file,
    flags,
    fds_path=None,
    debug=False
):
    """
    Execute every process test and validate within parameters given.

    It executes every process in a different independent thread.
    """
    try:
        processes = test_params['processes']
    except KeyError:
        logger.error(f'Missing processes in test parameters')
        return False

    thread_list = []
    result_list = [True]

    logger.debug(f'Preparing processes for test: {test_id}')

    # Read every process config and run it in different threads
    for process_config in processes:

        thread = threading.Thread(
            target=execute_validate_thread_test,
            args=(test_id,
                  result_list,
                  ds_tool_path,
                  process_config,
                  config_file,
                  flags,
                  fds_path,
                  debug))
        thread_list.append(thread)

    logger.debug(f'Executing process for test: {test_id}')

    # Start threads
    for thread in thread_list:
        thread.start()

    # Wait for all processes to end
    for thread in thread_list:
        thread.join()

    logger.debug(f'Finished all processes for test: {test_id}')

    result = functools.reduce(lambda a, b: a and b, result_list)

    val.print_result_bool(
        logger,
        f'Result for test {test_id}: ',
        result
    )

    return result


def run_tests(
    test_params,
    config_params,
    discovery_server_tool_path,
    tests=None,
    intraprocess=None,
    shm=None,
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
    # get all tests available in parameter file
    if tests is None:
        tests = [(t, t) for t in list(test_params.keys())]

    logger.debug(f'Different tests to execute: {tests}')

    # create configuration file
    # for now does only support intraprocess configuration
    try:
        config_intraprocess_on = (
            'INTRAPROCESS',
            config_params['transports']['intraprocess']['on']
        )
        config_intraprocess_off = (
            'INTRAPROCESS_OFF',
            config_params['transports']['intraprocess']['off']
        )

    except KeyError:
        logger.error(f'Incorrect transports in parameter configuration')

    if intraprocess is None:
        config_files = [config_intraprocess_on, config_intraprocess_off]
    elif intraprocess:
        config_files = [config_intraprocess_on]
    else:
        config_files = [config_intraprocess_off]

    logger.debug(f'Different configuration files to execute: {config_files}')

    # create flag parameters for DS tool execution
    # only shared memory flag supported so far
    # flag will be an list of flags, so combinatory must be list of lists
    if shm is None:
        flags_combinatory = [('NO_SHM', []), ('SHM', ['-s'])]
    elif shm:
        flags_combinatory = [('SHM', ['-s'])]
    else:
        flags_combinatory = [('NO_SHM', [])]

    logger.debug(f'Different flags to execute: {config_files}')

    test_results = True

    # iterate over parameters
    for test_name, test in tests:
        for config_name, config_file in config_files:
            for flag_name, flags in flags_combinatory:

                # Remove Databases if param <clear> set in test params in order
                # to execute same process again and not load old databases
                # Last database will stay except clear flag is set
                try:
                    if test_params[test]['clear']:
                        clear_db(os.getcwd())
                except KeyError:
                    pass

                test_id = '' + test_name
                test_id += '.' + config_name
                test_id += '.' + flag_name

                logger.info('--------------------------------------------')
                logger.info(f'Running {test}'
                            f' with config file <{config_file}>'
                            f' and flags {flags}')

                test_results = execute_validate_test(
                        test_name,
                        test_id,
                        discovery_server_tool_path,
                        test_params[test],
                        config_file,
                        flags,
                        fds_path,
                        debug) and test_results
                logger.info('--------------------------------------------')

    return test_results


def clear_db(wd):
    """
    Clear the working directory removing the generated database files.

    :param wd: The working directory where clear the generated files.
    """
    files = glob.glob(
        os.path.join(wd, '*.json'))
    files.extend(glob.glob(
        os.path.join(wd, '*.db')))

    logger.debug(f'Removing files: {files}')

    for f in files:
        try:
            os.remove(f)
        except OSError:
            logger.error(f'Error while deleting file: {f}')


def clear(wd):
    """
    Clear the working directory removing the generated files.

    :param wd: The working directory where clear the generated files.
    """
    files = glob.glob(
        os.path.join(wd, f'*.snapshot~'))
    files.extend(glob.glob(
        os.path.join(wd, '*.json')))
    files.extend(glob.glob(
        os.path.join(wd, '*.db')))

    logger.debug(f'Removing files: {files}')

    for f in files:
        try:
            os.remove(f)
        except OSError:
            logger.error(f'Error while deleting file: {f}')


def load_test_params(tests_params_path):
    """
    Load test parameters.

    :param tests_params_path: The path to the json file which contains the
        test parameters.

    :return: The Python dict resulting from reading the test params.
    """
    if os.path.isfile(tests_params_path):
        with open(tests_params_path) as json_file:
            json_dic = json.load(json_file)

        # modify paths
        shared.replace_string_dict(
            json_dic,
            '<CONFIG_RELATIVE_PATH>',
            os.path.dirname(tests_params_path)
        )

        try:
            test_params = json_dic['tests']
            config_params = json_dic['configurations']
        except KeyError:
            logger.error('XML configuration files not found')
            exit(1)

    else:
        logger.error('Not found file with test parameters.')
        exit(1)

    return test_params, config_params


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

    test_params, config_params = load_test_params(args.params)

    intraprocess = args.intraprocess
    if intraprocess is not None:
        intraprocess = shared.boolean_from_string(intraprocess)

    shm = args.shm
    if shm is not None:
        shm = shared.boolean_from_string(shm)

    result = run_tests(
        test_params,
        config_params,
        args.exe,
        tests=args.test,
        intraprocess=intraprocess,
        shm=shm,
        fds_path=(args.fds if args.fds else None),
        debug=args.debug
    )

    val.print_result_bool(
        logger,
        f'Overall test results: ',
        result
    )

    if not args.not_remove:
        clear(os.getcwd())

    if result:
        exit(0)
    else:
        exit(1)
