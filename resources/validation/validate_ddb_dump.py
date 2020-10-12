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

import json
import argparse 

from validation_ddb import validate_state

description="""Script to validate a DiscoveryDataBase json dump.
    Be aware that this validator will check if the ddb state is consistent
    at the time before populating the writers."""

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
        '-d',
        '--debug',
        action='store_true',
        help='Print step by step debug info'
    )
    parser.add_argument(
        '-f',
        '--file',
        type=str,
        required=True,
        help='Path to the json file containing the dumps data.'
    )
    parser.add_argument(
        '-i',
        default=0,
        type=int,
        required=False,
        help='Index of the dump data to analyze'
    )
    parser.add_argument(
        '-a',
        '--all',
        action='store_true',
        help='Process all the dump data in the file'
    )
    parser.add_argument(
        '-s',
        '--skip',
        action='store_true',
        help='Skip the rest of the process when it has been an error'
    )

    return parser.parse_args()


# return a vector of dictionaries. One for each json in file
def load_file(path):
    json_list = []
    print("Reading json file: " + path)
    with open(path) as f:
        for jsonObj in f:
            json_state = json.loads(jsonObj)
            json_list.append(json_state)
    return json_list


# validate all dicts
# if one is incorrect it cuts the execution
def validate_all(json_list, debug=False, skip=True):
    index = -1
    for i, d in enumerate(json_list):
        print (" Checking state " + str(i) + ".\n")
        if not validate_state(d, debug = debug, run_all = not skip):
            if skip:
                return False, i
            else:
                index = i
    return True, index


if __name__ == '__main__':

    # Parse arguments
    args = parse_options()

    json_list = load_file(args.file)

    if args.all:
        print ("Checking all states. Total " + str(len(json_list)) + " states.")
        res, index = validate_all(json_list, args.debug, args.skip)

    else:
        index = args.i
        print ("Checking state " + str(index) + ".")
        res = validate_state(json_list[index], debug = args.debug, run_all = not args.skip)
    
    if res:
        if index == -1:
            print ("DDB state is CORRECT for all indexes.")
        else:
            print ("DDB state is CORRECT for index " + str(index) + ".")
    else:
        print ("DDB state is NOT correct for index " + str(index) + ".")
    
