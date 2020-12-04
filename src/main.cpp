
#include "log/DSLog.h"
#include "version/config.h"

#include <fastdds/dds/log/StdoutErrConsumer.hpp>
#include <fastrtps/Domain.h>

#include "DSManager.h"

using namespace eprosima;
using namespace fastrtps;
using namespace fastdds;
using namespace fastrtps::rtps;
using namespace fastdds::rtps;
using namespace discovery_server;

std::pair<std::set<std::string>, std::string> validationCommandLineParser(int argc, char * argv[]);

int main(int argc, char * argv[])
{
    // Initialize loging
    #if defined LOG_LEVEL_INFO
        Log::SetVerbosity(Log::Kind::Info);
    #elif defined LOG_LEVEL_WARN
        Log::SetVerbosity(Log::Kind::Warning);
    #elif defined LOG_LEVEL_ERROR
        Log::SetVerbosity(Log::Kind::Error);
    #endif

    // Clear all the consumers.
    Log::ClearConsumers();

    // Create a StdoutErrConsumer consumer that logs entries to stderr only when the Log::Kind is equal to WARNING
    // This allows the test validate the output of the executions
    std::unique_ptr<eprosima::fastdds::dds::StdoutErrConsumer> stdouterr_consumer(
            new eprosima::fastdds::dds::StdoutErrConsumer());
    stdouterr_consumer->stderr_threshold(Log::Kind::Warning);

    // Register the consumer
    Log::RegisterConsumer(std::move(stdouterr_consumer));

    int return_code = 0;

    if (!(argc > 1))
    {
        std::cout << "Usage: discovery-server [CONFIG_XML|SNAPSHOT_XML+ [-out output_filename]]" << std::endl;
    }
    else if (argc == 2)
    {
        std::string path_to_config = argv[1];

        {
            DSManager manager(path_to_config);

            // Follow the config file instructions
            manager.runEvents(std::cin, std::cout);

            // maybe it's not a standalone test and validation should be procrastinated
            if (manager.shouldValidate())
            {
                // Check the snapshots taken
                if (!manager.validateAllSnapshots())
                {
                    LOG_ERROR("Discovery Server error: several snapshots show info leakage");
                    return_code = -1; // report CTest the test fail
                }
                else
                {
                    std::cout << manager.successMessage() << std::endl;
                }
            }
        }
    }
    else
    {
        auto arguments = validationCommandLineParser(argc, argv);
        DSManager manager(arguments.first, arguments.second);

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
    }

    Log::Flush();
    Domain::stopAll();

    return return_code;

}

// C++11 template deduction cannot directly map std::tolower in std::transform
char toLower(const char & c)
{
    return std::tolower(c);
}

std::pair<std::set<std::string>,std::string> validationCommandLineParser(int argc, char * argv[])
{
    using namespace std;

    // handle -out output_file_name scenario
    bool next_is_filename = false;
    string outfilename;
    string outflag("-out");

    set<string> files;
    for(int i = 1; i < argc; ++i)
    {
        const char * argtext = argv[i];

        // former argument was -out flag
        if(next_is_filename)
        {
            next_is_filename = false;
            outfilename = argtext;
        }
        else
        {
            // check if its a flag or a file
            string::size_type len = string::traits_type::length(argtext);
            string file(len, ' ');
            transform(argtext, argtext + len, file.begin(), toLower);

            if(outflag == file)
            {
                next_is_filename = true;
            }
            else
            {
                // kept the file
                files.emplace(argtext, len);
            }
        }
    }

    return std::make_pair(std::move(files),std::move(outfilename));
}
