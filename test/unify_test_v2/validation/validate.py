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
import os

import GroundTruthValidation as gtv


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
        help='Path to the xml file containing the test result snapshot.'
    )
    parser.add_argument(
        '-g',
        '--ground-truth',
        type=str,
        required=True,
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


if __name__ == '__main__':

    # Parse arguments
    args = parse_options()

    val = gtv.GroundTruthValidation(args.snapshot, args.ground_truth)

    val.validate()

    if args.save_dicts:
        val.save_generated_json_files(
            validate_json=True,
            ground_truth_json=True,
            validate_json_file=args.input_json,
            ground_truth_json_file=args.ground_truth_json)
