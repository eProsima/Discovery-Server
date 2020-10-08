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
"""."""
import argparse
import json

import xmltodict


def dict2list(d):
    """."""
    return d if isinstance(d, list) else [d]


def fill_validate_publisher_data(
    snapshot_dict,
    ptdi_guid_prefix,
    publisher_guid_entity,
    topic
):
    """."""
    publisher_guid = f'{ptdi_guid_prefix}.{publisher_guid_entity}'

    for snapshot in dict2list(snapshot_dict['DS_Snapshots']['DS_Snapshot']):
        for ptdb in dict2list(snapshot['ptdb']):
            for ptdi in dict2list(ptdb['ptdi']):
                if (ptdi_guid_prefix != ptdi['@guid_prefix'] and
                        'subscriber' in (x.lower() for x in ptdi.keys())):
                    for sub in dict2list(ptdi['subscriber']):
                        if topic == sub['@topic']:
                            if f'ptdi_{ptdi_guid_prefix}' not in ptdb.keys():
                                validate_dict[
                                    'DS_Snapshots'][
                                    f"DS_Snapshot_{snapshot['@timestamp']}"][
                                    f"ptdb_{ptdb['@guid_prefix']}"][
                                    f'ptdi_{ptdi_guid_prefix}'] = {
                                        'guid_prefix': ptdi_guid_prefix
                                    }

                            validate_dict[
                                'DS_Snapshots'][
                                f"DS_Snapshot_{snapshot['@timestamp']}"][
                                f"ptdb_{ptdb['@guid_prefix']}"][
                                f'ptdi_{ptdi_guid_prefix}'][
                                f'publisher_{publisher_guid}'] = {
                                        'topic': topic,
                                        'guid': publisher_guid
                                }


def fill_validate_subscriber_data(
    snapshot_dict,
    ptdi_guid_prefix,
    subscriber_guid_entity,
    topic
):
    """."""
    subscriber_guid = f'{ptdi_guid_prefix}.{subscriber_guid_entity}'

    for snapshot in dict2list(snapshot_dict['DS_Snapshots']['DS_Snapshot']):
        for ptdb in dict2list(snapshot['ptdb']):
            for ptdi in dict2list(ptdb['ptdi']):
                if (ptdi_guid_prefix != ptdi['@guid_prefix'] and
                        'publisher' in (x.lower() for x in ptdi.keys())):
                    for pub in dict2list(ptdi['publisher']):
                        if topic == pub['@topic']:
                            if f'ptdi_{ptdi_guid_prefix}' not in ptdb.keys():
                                validate_dict[
                                    'DS_Snapshots'][
                                    f"DS_Snapshot_{snapshot['@timestamp']}"][
                                    f"ptdb_{ptdb['@guid_prefix']}"][
                                    f'ptdi_{ptdi_guid_prefix}'] = {
                                        'guid_prefix': ptdi_guid_prefix
                                    }

                            validate_dict[
                                'DS_Snapshots'][
                                f"DS_Snapshot_{snapshot['@timestamp']}"][
                                f"ptdb_{ptdb['@guid_prefix']}"][
                                f'ptdi_{ptdi_guid_prefix}'][
                                f'subscriber_{subscriber_guid}'] = {
                                        'topic': topic,
                                        'guid': subscriber_guid
                                }


def parse_xml_snapshot(xml_file_path):
    """."""
    with open(xml_file_path) as xml_file:
        snapshot_dict = xmltodict.parse(xml_file.read())

    return snapshot_dict


def create_copy_and_validate_dict(snapshot_dict):
    """."""
    for snapshot in dict2list(snapshot_dict['DS_Snapshots']['DS_Snapshot']):
        copy_dict[
            'DS_Snapshots'][
            f"DS_Snapshot_{snapshot['@timestamp']}"] = {}
        validate_dict[
            'DS_Snapshots'][
            f"DS_Snapshot_{snapshot['@timestamp']}"] = {}
        for ptdb in snapshot['ptdb']:
            copy_dict[
                'DS_Snapshots'][
                f"DS_Snapshot_{snapshot['@timestamp']}"][
                f"ptdb_{ptdb['@guid_prefix']}"] = {
                    'guid_prefix': ptdb['@guid_prefix']}
            validate_dict[
                'DS_Snapshots'][
                f"DS_Snapshot_{snapshot['@timestamp']}"][
                f"ptdb_{ptdb['@guid_prefix']}"] = {
                    'guid_prefix': ptdb['@guid_prefix']}

            for ptdi in ptdb['ptdi']:
                if ptdi['@server'] == 'true':
                    continue

                copy_dict[
                    'DS_Snapshots'][
                    f"DS_Snapshot_{snapshot['@timestamp']}"][
                    f"ptdb_{ptdb['@guid_prefix']}"][
                    f"ptdi_{ptdi['@guid_prefix']}"] = {
                        'guid_prefix': ptdi['@guid_prefix']}

                if (ptdi['@guid_prefix'] == ptdb['@guid_prefix']):
                    validate_dict[
                        'DS_Snapshots'][
                        f"DS_Snapshot_{snapshot['@timestamp']}"][
                        f"ptdb_{ptdb['@guid_prefix']}"][
                        f"ptdi_{ptdi['@guid_prefix']}"] = {
                            'guid_prefix': ptdi['@guid_prefix']}

                if 'publisher' in (x.lower() for x in ptdi.keys()):
                    for pub in dict2list(ptdi['publisher']):
                        publisher_guid = f"{pub['@guid_prefix']}. \
                            {pub['@guid_entity']}"
                        copy_dict[
                            'DS_Snapshots'][
                            f"DS_Snapshot_{snapshot['@timestamp']}"][
                            f"ptdb_{ptdb['@guid_prefix']}"][
                            f"ptdi_{ptdi['@guid_prefix']}"][
                            f'publisher_{publisher_guid}'] = {
                                'topic': pub['@topic'],
                                'guid': publisher_guid
                            }
                        if (ptdi['@guid_prefix'] == ptdb['@guid_prefix']):
                            validate_dict[
                                'DS_Snapshots'][
                                f"DS_Snapshot_{snapshot['@timestamp']}"][
                                f"ptdb_{ptdb['@guid_prefix']}"][
                                f"ptdi_{ptdi['@guid_prefix']}"][
                                f'publisher_{publisher_guid}'] = {
                                    'topic': pub['@topic'],
                                    'guid': publisher_guid
                                }

                if 'subscriber' in (x.lower() for x in ptdi.keys()):
                    for sub in dict2list(ptdi['subscriber']):
                        subscriber_guid = f"{sub['@guid_prefix']}. \
                            {sub['@guid_entity']}"
                        copy_dict[
                            'DS_Snapshots'][
                            f"DS_Snapshot_{snapshot['@timestamp']}"][
                            f"ptdb_{ptdb['@guid_prefix']}"][
                            f"ptdi_{ptdi['@guid_prefix']}"][
                            f'subcriber_{subscriber_guid}'] = {
                                'topic': sub['@topic'],
                                'guid': subscriber_guid
                            }
                        if (ptdi['@guid_prefix'] == ptdb['@guid_prefix']):
                            validate_dict[
                                'DS_Snapshots'][
                                f"DS_Snapshot_{snapshot['@timestamp']}"][
                                f"ptdb_{ptdb['@guid_prefix']}"][
                                f"ptdi_{ptdi['@guid_prefix']}"][
                                f'subcriber_{subscriber_guid}'] = {
                                    'topic': sub['@topic'],
                                    'guid': subscriber_guid
                                }


def process_validation_dict(snapshot_dict):
    """."""
    for snapshot in dict2list(snapshot_dict['DS_Snapshots']['DS_Snapshot']):
        for ptdb in snapshot['ptdb']:
            for ptdi in ptdb['ptdi']:
                if ptdi['@server'] == 'true':
                    continue

                if 'publisher' in (x.lower() for x in ptdi.keys()):
                    for pub in dict2list(ptdi['publisher']):
                        fill_validate_publisher_data(
                                snapshot_dict,
                                ptdi['@guid_prefix'],
                                pub['@guid_entity'],
                                pub['@topic'])

                if 'subscriber' in (x.lower() for x in ptdi.keys()):
                    for sub in dict2list(ptdi['subscriber']):
                        fill_validate_subscriber_data(
                                snapshot_dict,
                                ptdi['@guid_prefix'],
                                sub['@guid_entity'],
                                sub['@topic'])


def write_json_snapshots():
    """."""
    copy_dict_str = json.dumps(copy_dict, indent=4)
    with open("copy_dict.json", "w") as cp_file:
        cp_file.write(copy_dict_str)
        cp_file.close()

    val_dict_str = json.dumps(validate_dict, indent=4)
    with open("validation_dict.json", "w") as val_file:
        val_file.write(val_dict_str)
        val_file.close()


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

    return parser.parse_args()


if __name__ == '__main__':

    # Parse arguments
    args = parse_options()

    snapshot_dict = parse_xml_snapshot(args.snapshot)

    copy_dict = {'DS_Snapshots': {}}
    validate_dict = {'DS_Snapshots': {}}

    create_copy_and_validate_dict(snapshot_dict)

    process_validation_dict(snapshot_dict)

    valid = copy_dict == validate_dict

    if valid:
        print('The test results are correct.')
    else:
        print('The test results are NOT correct.')

    # print(json.dumps(copy_dict, indent=4))
    # print(json.dumps(validate_dict, indent=4))
