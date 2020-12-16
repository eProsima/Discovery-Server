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
Script to execute and validate Discovery Server v2 tests.

This script could be executed for a single test, but the default configuration
will launch every single test (in parameter file) with different
configurations combined.
"""
import argparse
import functools
import glob
import itertools
import json
import logging
import os
import signal
import subprocess
import threading
import time

import shared.shared as shared

import validation.validation as val

DESCRIPTION = """Script to execute and validate Discovery Server v2 test"""
USAGE = ('python3 run_test.py -e <path/to/discovery-server/executable>'
         ' -p <path/to/test/parameteres/file> [-t LIST[test-name]]'
         ' [-f <path/to/fastd)ds/tool>] [-d] [-r] [-i <bool>] [-s <bool>]')

# Max default time to kill a process in case it gets stucked
# This is done by ctest automatically, but this script could be
# run independently
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
        nargs='+',
        help='Name of the test case to execute (without xml extension), '
             'or a pattern that fits in a name test'
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
    flags_in,
    fds_path=None,
    debug=False
):
    """
    Execute a single process inside a test and validate it.

    This function will get all the needed information from a process
    paramenter json object, and will creat the environment variables, the
    arguments and the initial and final time for a single process.
    It will execute the process and validate it afterwards.

    :param test_id: TODO
    :return: True if everything OK, False if any error or if test did not pass
    """
    ##########################
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

    # Check whether it must execute the DS tool or the fastdds tool
    xml_config_file = None
    result_file = None
    if 'xml_config_file' in process_params.keys():
        # DS tool with xml config file
        xml_config_file = process_params['xml_config_file']
        result_file = val.validation_file_name(test_id)

    elif 'tool_config' in process_params.keys():
        # fastdds tool
        server_id = 0
        server_address = None
        server_port = None
        if fds_path is None:
            logger.error(f'Non tool given and needed')
            result_list.append(False)
            return False

        try:
            # Try to set args for fastdds tool
            # If any is missing could not be an error
            server_id = process_params['tool_config']['id']
            server_address = process_params['tool_config']['address']
            server_port = process_params['tool_config']['port']
        except KeyError:
            pass
    else:
        logger.error(f'Incorrect process paramenters: {test_id}-{process_id}')
        result_list.append(False)
        return False

    # Flags to add to command
    flags = [] + flags_in  # avoid list copy
    if 'flags' in process_params.keys():
        logger.debug(f'Adding flags to execution: {process_params["flags"]}')
        flags.extend(process_params['flags'])

    ###########
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
        # Create args with config file and outputfile
        process_args = \
            [ds_tool_path, '-c', xml_config_file, '-o', result_file] + flags

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
    proc = subprocess.Popen(
        process_args,
        env=my_env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )

    # Wait 5 seconds before killing the external client
    try:
        proc.wait(kill_time)
    except subprocess.TimeoutExpired:
        proc.send_signal(signal.SIGINT)

    # In case SIGINT has failed, it waits a fraction of KILL_TIME and
    # kill the process badly
    try:
        proc.wait(kill_time/2)
    except subprocess.TimeoutExpired:
        proc.kill()

    # Do not use communicate, as stderr is needed further in validation

    ############
    # VALIDATION

    # Show process output
    logger.debug(f'Process {test_id} output')
    stderr_lines = proc.stderr.readlines()
    for line in stderr_lines:
        logger.info(line)
    for line in proc.stdout.readlines():
        logger.debug(line)

    # Check if validation needed by test params
    if 'validation' not in process_params.keys():
        result_list.append(True)
        return True

    # Validation needed, pass to validate_test function
    logger.debug(f'Executing validation for process {process_id} '
                 f'in test {test_id}')
    validation_params = process_params['validation']
    process_name = test_id + '.process_' + str(process_id)
    validator_input = val.ValidatorInput(
        proc.returncode,
        len(stderr_lines),
        result_file
    )

    # Call validate_test to validate with every validator in parameters
    result = val.validate_test(
        process_name,
        validation_params,
        validator_input,
        debug,
        logger
    )

    # Update result_list and return
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
    It launch every process and wait till all of them are finished.
    Afterwards, it checks whether all processes has finished successfully

    :param test_name:TODO

    :return: True if everything OK, False if any error or if test did not pass
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


def get_configurations(config_params, intraprocess, shm):
    """ TODO """
    # Get configuration files
    config_files = []
    try:
        configs = config_params['configuration_files']
        for k, v in configs.items():
            config_files.append((k, v))
    except KeyError as e:
        logger.debug(e)

    if intraprocess is not None:
        if intraprocess:
            config_files = \
                [c for c in config_files if c[0] == 'INTRAPROCESS_ON']
        else:
            config_files = \
                [c for c in config_files if c[0] == 'INTRAPROCESS_OFF']

    # If no configuration files given, create CONF by default
    if len(config_files) == 0:
        config_files.append(('NO_CONFIG', ''))

    logger.debug(f'Different configuration files to execute: '
                 f'{[c[0] for c in config_files]}')

    # create flag parameters for DS tool execution
    # only shared memory flag supported so far
    # flag will be an list of flags, so combinatory must be list of lists
    flags = []
    try:
        fl = config_params['flags']
        for k, v in fl.items():
            flags.append((k, v))
    except KeyError as e:
        logger.debug(e)

    if shm is not None and not shm:
        flags = [f for f in flags if f[0] == 'SHM_OFF']
    if shm is not None and shm:
        flags = [f for f in flags if f[0] != 'SHM_OFF']

    flags_combinatory = []
    for i in range(1, 1+len(flags)):
        for combination in itertools.combinations(flags, i):
            comb_name = '.'.join([c[0] for c in combination])
            comb_flags = [c[1] for c in combination]
            flags_combinatory.append((comb_name, comb_flags))

    if len(flags_combinatory) == 0 or shm is None:
        flags_combinatory.append(('NO_FLAGS', []))

    logger.debug(f'Different flags to execute: '
                 f'{[f[0] for f in flags_combinatory]}')

    return config_files, flags_combinatory


def create_tests(
    params_file,
    config_params,
    discovery_server_tool_path,
    tests=None,
    intraprocess=None,
    shm=None,
    fds_path=None,
    debug=False,
):
    """
    Execute the tests and validate the output.

    :param params_file_df: TODO

    :return: True if all the tests passed the validation, False otherwise.
    """
    # get all tests available in parameter file
    if tests is None:
        tests = set(params_file.keys())
    else:
        ex_tests = set()
        for t in tests:
            # Add any test matching this string
            ex_tests.update(
                [test for test in params_file.keys() if test.find(t) != -1]
            )
        tests = ex_tests

    # Get Test file and name. Duplicated to follow other parameter
    # configuration patterns
    tests = [(t, t) for t in sorted(tests)]

    logger.debug(f'Different tests to execute: {[t[0] for t in tests]}')

    # Get configurations
    config_files, flags_combinatory = get_configurations(
        config_params,
        intraprocess,
        shm
    )

    test_results = True

    # iterate over parameters
    for test_name, test in tests:
        for config_name, config_file in config_files:
            for flag_name, flags in flags_combinatory:

                # Remove Databases if param <clear> set in test params in order
                # to execute same process again and not load old databases
                # Last database will stay except clear flag is set
                try:
                    if params_file[test]['clear']:
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
                        params_file[test],
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

        # Modify relative paths
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

    result = create_tests(
        test_params,
        config_params,
        args.exe,
        tests=args.test,
        intraprocess=intraprocess,
        shm=shm,
        fds_path=(args.fds if args.fds else None),
        debug=args.debug,
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
