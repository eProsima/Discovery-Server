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
import json

import jsondiff

import xmltodict


def str2bool(arg):
    """
    Convert a string argument to boolean.

    :param arg: The string argument.
    :raise: argparse.ArgumentTypeError if the argument is not valid.
    """
    if isinstance(arg, bool):
        return arg
    if arg.lower() in ('yes', 'true', 't', 'y', '1'):
        return True
    elif arg.lower() in ('no', 'false', 'f', 'n', '0'):
        return False
    else:
        raise argparse.ArgumentTypeError('Boolean value expected.')


def dict2list(d):
    """
    Cast an item from a dictionary to a list if it is not already one.

    :param d: The dictionary item.
    :return: The list with the dictionary items as elements of the list.
    """
    return d if isinstance(d, list) else [d]


def fill_matching_publisher_data(
    snapshot_dict,
    ptdi_guid_prefix,
    publisher_guid_entity,
    topic
):
    """
    Set the publisher data which match the subscriber topic.

    Search for participants with subscribers on the same topic as the
    publisher and add the remote publisher's data to the local participant.

    :param snapshot_dict: The snapshot dictionary.
    :param ptdi_guid_prefix: The remote participant guid prefix.
    :param publisher_guid_entity: The remote publisher guid entity.
    :param topic: The publisher topic.
    """
    publisher_guid = f'{ptdi_guid_prefix}.{publisher_guid_entity}'

    for snapshot in dict2list(snapshot_dict['DS_Snapshots']['DS_Snapshot']):
        for ptdb in dict2list(snapshot['ptdb']):
            if ptdi_guid_prefix != ptdb['@guid_prefix']:
                for ptdi in dict2list(ptdb['ptdi']):
                    if'subscriber' in (x.lower() for x in ptdi.keys()):
                        for sub in dict2list(ptdi['subscriber']):
                            if topic == sub['@topic']:
                                v_ptdb = validate_dict[
                                        'DS_Snapshots'][
                                        ('DS_Snapshot_'
                                            f"{snapshot['@timestamp']}")][
                                        f"ptdb_{ptdb['@guid_prefix']}"]
                                if (f'ptdi_{ptdi_guid_prefix}'
                                        not in v_ptdb.keys()):
                                    validate_dict[
                                        'DS_Snapshots'][
                                        ('DS_Snapshot_'
                                            f"{snapshot['@timestamp']}")][
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


def fill_matching_subscriber_data(
    snapshot_dict,
    ptdi_guid_prefix,
    subscriber_guid_entity,
    topic
):
    """
    Set the subscribers data which match the publisher topic.

    Search for participants with publishers on the same topic as the
    subscriber and add the remote subscriber's data to the local participant.

    :param snapshot_dict: The snapshot dictionary.
    :param ptdi_guid_prefix: The remote participant guid prefix.
    :param subscriber_guid_entity: The remote subscriber guid entity.
    :param topic: The subscriber topic.
    """
    subscriber_guid = f'{ptdi_guid_prefix}.{subscriber_guid_entity}'

    for snapshot in dict2list(snapshot_dict['DS_Snapshots']['DS_Snapshot']):
        for ptdb in dict2list(snapshot['ptdb']):
            if ptdi_guid_prefix != ptdb['@guid_prefix']:
                for ptdi in dict2list(ptdb['ptdi']):
                    if 'publisher' in (x.lower() for x in ptdi.keys()):
                        for pub in dict2list(ptdi['publisher']):
                            if topic == pub['@topic']:
                                v_ptdb = validate_dict[
                                        'DS_Snapshots'][
                                        ('DS_Snapshot_'
                                            f"{snapshot['@timestamp']}")][
                                        f"ptdb_{ptdb['@guid_prefix']}"]
                                if (f'ptdi_{ptdi_guid_prefix}'
                                        not in v_ptdb.keys()):
                                    validate_dict[
                                        'DS_Snapshots'][
                                        ('DS_Snapshot_'
                                            f"{snapshot['@timestamp']}")][
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
    """
    Read an xml snapshot file and convert it into a dictionary.

    :param xml_file_path: The path to the xml snapshot.
    """
    with open(xml_file_path) as xml_file:
        snapshot_dict = xmltodict.parse(xml_file.read())

    return snapshot_dict


def create_copy_and_validate_dict(snapshot_dict):
    """
    Create the copy and validation dictionaries parsing the original snapshot.

    The copy dictionary contains the relevant elements of the original
    snapshot dictionary. Each element of this original dictionary is parsed
    to be unique in the new copy dictionary. On the other hand, the basic
    structure of the validation dictionary is created. This basic structure is
    the participants (ptdi) and endpoints (publisher/subscriber) instances of
    a local participant (ptdb).
    These elements are not dependent on the other participants and can
    therefore be filled in the first parse of the original snapshot dictionary.

    :param snapshot_dict: The original snapshot dictionary.
    """
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
                        publisher_guid = '{}.{}'.format(
                            pub['@guid_prefix'], pub['@guid_entity'])
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
                        subscriber_guid = '{}.{}'.format(
                            sub['@guid_prefix'], sub['@guid_entity'])
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
    """Set the validation dictionary with the remote known participants.

    This function iterates over the validation dictionary to map and set
    known remote participants along with their publishers and subscribers.

    :param snapshot_dict: The snapshot dictionary.
    """
    for snapshot in dict2list(snapshot_dict['DS_Snapshots']['DS_Snapshot']):
        for ptdb in snapshot['ptdb']:
            for ptdi in ptdb['ptdi']:
                if ptdi['@server'] == 'true':
                    continue

                if 'publisher' in (x.lower() for x in ptdi.keys()):
                    for pub in dict2list(ptdi['publisher']):
                        fill_matching_publisher_data(
                                snapshot_dict,
                                ptdi['@guid_prefix'],
                                pub['@guid_entity'],
                                pub['@topic'])

                if 'subscriber' in (x.lower() for x in ptdi.keys()):
                    for sub in dict2list(ptdi['subscriber']):
                        fill_matching_subscriber_data(
                                snapshot_dict,
                                ptdi['@guid_prefix'],
                                sub['@guid_entity'],
                                sub['@topic'])


def write_json_file(
    data_dict,
    json_file_path
):
    """
    Write a dictionary in a json file.

    :param data_dict: The dictionary object.
    :param json_file_path: The path to the json file.
    """
    data_dict_str = json.dumps(data_dict, indent=4)
    with open(json_file_path, 'w') as cp_file:
        cp_file.write(data_dict_str)


def dict_equal(dict_a, dict_b):
    """
    Check if true dictionaries are equal.

    :param dict_a: The first disctionary.
    :param dict_b: The second disctionary.
    :return: True if dict_a is equal to dict_b.
    """
    json_a = json.dumps(dict_a, sort_keys=True)
    json_b = json.dumps(dict_b, sort_keys=True)

    return jsondiff.diff(json_a, json_b)


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
        type=str2bool,
        nargs='?',
        const=True,
        default=False,
        help='Save the generated dictionaries in json files.'
    )
    parser.add_argument(
        '-c',
        '--copy-dict',
        default='./copy_dict.json',
        type=str,
        required=False,
        help='Path to the xml file containing the snapshot data.'
    )
    parser.add_argument(
        '-v',
        '--val-dict',
        default='./validation_dict.json',
        type=str,
        required=False,
        help='Path to the xml file containing the snapshot data.'
    )

    return parser.parse_args()


if __name__ == '__main__':

    # Parse arguments
    args = parse_options()

    snapshot_dict = parse_xml_snapshot(args.snapshot)
    write_json_file(snapshot_dict, 'parsed_dict.json')

    copy_dict = {'DS_Snapshots': {}}
    validate_dict = {'DS_Snapshots': {}}

    create_copy_and_validate_dict(snapshot_dict)

    process_validation_dict(snapshot_dict)

    if dict_equal(copy_dict, validate_dict):
        print('The test results are correct.')
    else:
        print('The test results are NOT correct.')

    if (args.save_dicts):
        write_json_file(copy_dict, args.copy_dict)
        write_json_file(validate_dict, args.val_dict)
