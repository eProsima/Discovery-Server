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
"""Script to validate an snapshot resulting from a Discovery-Server test."""
import argparse

import shared

import validation


def parse_options():
    """
    Parse arguments.

    :return: The arguments parsed.
    """
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        description="""
            Script to validate an xml snapshot resulting from a
            Discovery-Server test application.
        """
    )
    parser.add_argument(
        '-d',
        '--debug',
        action='store_true',
        help='Print debug info.'
    )
    parser.add_argument(
        '-s',
        '--snapshot',
        type=str,
        required=True,
        help='Path to the xml file containing the snapshot data.'
    )
    parser.add_argument(
        '--save-dicts',
        action='store_true',
        help='Save the generated dictionaries in json files.'
    )
    parser.add_argument(
        '--snapshot-json',
        default='./parsed_snapshot.json',
        type=str,
        required=False,
        help='Path to the json file containing the snapshot data.'
    )
    parser.add_argument(
        '-c',
        '--copy-json',
        default='./copy_snapshot.json',
        type=str,
        required=False,
        help='Path to the generated json file containing the snapshot data.'
    )
    parser.add_argument(
        '-v',
        '--val-json',
        default='./validate_snapshot.json',
        type=str,
        required=False,
        help='Path to the validation json file containing the snapshot data.'
    )

    return parser.parse_args()


if __name__ == '__main__':

    # Parse arguments
    args = parse_options()

    val = validation.Validation(args.snapshot)

    if val.validate():
        print(
            f'Test result: {shared.bcolors.OK}PASS{shared.bcolors.ENDC}')
    else:
        print(f'Test result: {shared.bcolors.FAIL}FAIL{shared.bcolors.ENDC}')

    if args.save_dicts:
        val.save_generated_json_files(
            parsed_json=True,
            copy_json=True,
            validate_json=True,
            parsed_json_file=args.snapshot_json,
            copy_json_file=args.copy_json,
            validate_json_file=args.val_json
        )
