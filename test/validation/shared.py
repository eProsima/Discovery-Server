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
"""Module to provide common methods for the test validation tool."""
import argparse
import os
from enum import Enum


class bcolors:
    """Colors definition."""

    OK = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    BOLD = '\033[1m'
    ENDC = '\033[0m'


class ReturnCode(Enum):
    """Test return definition."""

    OK = 0
    SKIP = 1
    ERROR = 2
    FAIL = 3


def get_file_extension(file):
    """
    Get the extension of a file.

    :param file: The input file
    :return: The file extension
    """
    return os.path.splitext(str(file))[-1]


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


def is_valid_path(path):
    """
    Check whether <path> is a valid and existing path.

    :param directory: The directory path.
    :return: The path without ending .
    """
    path = str(path)
    if path.endswith('/'):
        path = path[:-1]
    if os.path.isdir(path) or os.path.isfile(path):
        return True, os.path.abspath(path)
    else:
        return False, path


def parent_dir_path():
    """Get the parent path of the current working directory."""
    return os.path.dirname(os.path.join(os.getcwd(), __file__))
