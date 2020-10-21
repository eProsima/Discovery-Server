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

import argparse
import os
import subprocess

description = """Script to execute every test in 'test_cases' subfolder and check it is working correctly."""

def parse_options():
    """
    Parse arguments.
    :return: The arguments parsed.
    """
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        description=description
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
        '--ntest',
        nargs='*',
        type=str,
        help='List of names of test case to execute (only name without xml). No set to test all.'
    )
    parser.add_argument(
        '-d',
        '--debug',
        action='store_true',
        help='Print test execution.'
    )

    return parser.parse_args()

# get the direction of the upper folder where this tests and subfolders are
def parent_dir_path():
    return os.path.dirname(os.path.join(os.getcwd(), __file__))

# return a list of tuples <name of test, path>
# return every test in test_cases if empty argument
def path_to_test(test=None):

    p_dir_path = parent_dir_path()
    test_cases_path = os.path.join(p_dir_path, "test_cases")

    tests = [f for f in os.listdir(test_cases_path) if (
        os.path.isfile(os.path.join(test_cases_path, f))
        and not f.startswith('_'))]

    # prevent to try to run a test that does not exist
    tests = [
        (".".join(t.split(".")[:-1]), os.path.join(test_cases_path, t))
        for t in tests]

    if test is not None:
        tests = [f for f in tests if f[0] in test]

    return tests

# execute test
def execute_test(test, path, discovery_server_tool_path, debug=False):
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

# TODO implement a validator that does not count lines but analyze the internal info
# validate the result test with the a priori solution
def check_test(test):
    result_file_path = os.path.join(os.getcwd(), test + ".snapshot~")
    lines_get = int(subprocess.check_output(["wc", "-l", result_file_path]).split()[0])
    expected_file_path = os.path.join(parent_dir_path(), "test_solutions/" + test + ".snapshot")
    lines_expected = int(subprocess.check_output(["wc", "-l", expected_file_path]).split()[0])
    os.system ("rm " + result_file_path)
    return lines_get == lines_expected

# execute the thread and validate the result
def validate_test(test, test_path, discovery_server_tool_path, debug=False):
    execute_test(test, test_path, discovery_server_tool_path, debug)
    return check_test(test)


if __name__ == '__main__':

    # Parse arguments
    args = parse_options()

    tests = path_to_test(args.ntest)

    for test, test_path in tests:
        if validate_test(test, test_path, args.exe, debug=args.debug):
            print ("Test " + test + " >> " + '\033[92m' + "OK" + '\033[0m')
        else:
            print ("Test " + test + " >> " + '\033[91m' + "FAIL" + '\033[0m')
