// Copyright 2020 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DS_ARGUMENTS_H_
#define DS_ARGUMENTS_H_

// Parsing setup
#include "optionparser.h"

enum  optionIndex
{
    UNKNOWN,
    HELP,
    CONFIG_FILE,
    PROPERTIES_FILE,
    OUTPUT_FILE,
    SHM
};

struct Arg : public option::Arg
{
    static option::ArgStatus check_inp(
            const option::Option& option,
            bool msg);
};

const option::Descriptor usage[] = {

    { UNKNOWN,   0, "",   "",             Arg::None,
      "\neProsima Discovery Server configuration execution tool\n"
      "\nUsage: tool -c donfig_file.xml [-s]\n" },

    { HELP,      0, "h",  "help",         Arg::None,
      "  -h  \t--help       Produce help message.\n" },

    { CONFIG_FILE,  0, "c", "config-file",    Arg::check_inp,
      "  -c \t--config-file  Mandatory configuration file path\n"},

    { PROPERTIES_FILE,  0, "p", "props-file",    Arg::check_inp,
      "  -p \t--props-file  Optional participant properties configuration file path\n"},

    { OUTPUT_FILE,  0, "o", "output-file",    Arg::check_inp,
      "  -o \t--output-file  File to write result snapshots. If not specified"
      " snapshots will be written in the file specified in the snapshot\n"},

    { SHM,    0, "s",  "disabled-shared-memory",       Arg::None,
      "  -s \t--shared-memory     Disable Shared Memory.\n" },

    { 0, 0, 0, 0, 0, 0 }
};

#endif // DS_ARGUMENTS_H_
