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

#include "log/DSLog.h"
#include "version/config.h"

#if FASTRTPS_VERSION_MAJOR >= 2 || (FASTRTPS_VERSION_MAJOR == 2 && FASTRTPS_VERSION_MINOR >= 1)
#include <fastdds/dds/log/StdoutErrConsumer.hpp>
#endif
#include <fastrtps/Domain.h>
#include <fastrtps/xmlparser/XMLProfileManager.h>

#include "DSManager.h"
#include "arguments.h"

using namespace eprosima;
using namespace fastrtps;
using namespace fastdds;
using namespace fastrtps::rtps;
using namespace fastdds::rtps;
using namespace discovery_server;

using namespace std;

int main(int argc, char * argv[])
{
    // Clear all the consumers.
    Log::ClearConsumers();

    // Initialize loging
    #if LOG_LEVEL_INFO
        Log::SetVerbosity(Log::Kind::Info);
    #elif LOG_LEVEL_WARN
        Log::SetVerbosity(Log::Kind::Warning);
    #elif LOG_LEVEL_ERROR
        Log::SetVerbosity(Log::Kind::Error);
    #else
        Log::SetVerbosity(Log::Kind::Error);
    #endif

#if FASTRTPS_VERSION_MAJOR >= 2 && FASTRTPS_VERSION_MINOR >= 1
    // Create a StdoutErrConsumer consumer that logs entries to stderr only when the Log::Kind is equal to WARNING
    // This allows the test validate the output of the executions
    std::unique_ptr<eprosima::fastdds::dds::StdoutErrConsumer> stdouterr_consumer(
            new eprosima::fastdds::dds::StdoutErrConsumer());
    stdouterr_consumer->stderr_threshold(Log::Kind::Warning);

    // Register the consumer
    Log::RegisterConsumer(std::move(stdouterr_consumer));
#endif

    // skip program name argv[0] if present
    argc -= (argc > 0);
    argv += (argc > 0);
    option::Stats stats(usage, argc, argv);
    vector<option::Option> options(stats.options_max);
    vector<option::Option> buffer(stats.buffer_max);
    option::Parser parse(usage, argc, argv, &options[0], &buffer[0]);

    // check the command line options
    if (parse.error())
    {
        return 1;
    }

    // no arguments beyond options
    int noopts = parse.nonOptionsCount();
    if ( noopts )
    {
        string sep( noopts == 1 ? "argument: " : "arguments: " );

        cout << "Unknown ";

        while ( noopts-- )
        {
            cout << sep << parse.nonOption(noopts);
            sep = ", ";
        }

        endl(cout);

        return 1;
    }

    // show help if asked to
    if (options[HELP] || argc == 0)
    {
        option::printUsage(std::cout, usage);

        return 0;
    }

    // Load config file path from arg
    option::Option* pOp = options[CONFIG_FILE];

    if ( nullptr == pOp )
    {
        cout << "Specify configuration file is mandatory: use -c or --config-file option." << endl;
        return 1;
    }
    else if ( pOp->count() != 1)
    {
        cout << "Only one configuration file can be specified." << endl;
        return 1;
    }

    int return_code = 0;
    std::string path_to_config = pOp->arg;

    // Load Default XML files
    eprosima::fastrtps::xmlparser::XMLProfileManager::loadDefaultXMLFile();

    // Create DSManager
    DSManager manager(path_to_config, options[SHM]);
    if (!manager.correctly_created())
    {
        return_code = 1;
    }

    // Load output file path
    option::Option* pOp_of = options[OUTPUT_FILE];
    if ( nullptr != pOp_of )
    {
        manager.output_file(pOp_of->arg);
    }

    // Follow the config file instructions
    manager.runEvents(std::cin, std::cout);

    // Check the snapshots read
    if(manager.shouldValidate())
    {
        if(!manager.validateAllSnapshots())
        {
            LOG_ERROR("Discovery Server error: several snapshots show info leakage");
            return_code = -1; // report CTest the test fail
        }
        else
        {
            std::cout << manager.successMessage() << std::endl;
        }
    }

    Log::Flush();
    Domain::stopAll();

    return return_code;
}

/*static*/
option::ArgStatus Arg::check_inp(
        const option::Option& option,
        bool /* msg */)
{
    // the argument is required
    if ( nullptr != option.arg )
    {
        return option::ARG_OK;
    }

    return option::ARG_ILLEGAL;
}
