
#include "log/DSLog.h"
#include "version/config.h"

#include <fastrtps/Domain.h>

#include "DSManager.h"

using namespace eprosima;
using namespace fastrtps;
using namespace rtps;
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

    Log::SetCategoryFilter(std::regex("(RTPS_PARTICIPANT)|(DISCOVERY_SERVER)|(SERVER_PDP_THREAD)|(CLIENT_PDP_THREAD)"));

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
                    std::cout << manager.successMessage() << endl;
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
                std::cout << manager.successMessage() << endl;
            }
        }
    }

    Log::Flush();
    Domain::stopAll();
    
    return return_code;

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
            transform(argtext, argtext + len, file.begin(), std::tolower);

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