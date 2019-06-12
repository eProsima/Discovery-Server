
#include "log/DSLog.h"
#include "version/config.h"

#include <fastrtps/Domain.h>

#include "DSManager.h"

using namespace eprosima;
using namespace fastrtps;
using namespace rtps;
using namespace discovery_server;

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

    Log::SetCategoryFilter(std::regex("(DISCOVERY_SERVER)|(SERVER_PDP_THREAD)|(CLIENT_PDP_THREAD)"));
    // Log::SetCategoryFilter(std::regex("(SERVER_PDP_THREAD)|(CLIENT_PDP_THREAD)"));

    if (!(argc > 1))
    {
        std::cout << "Usage: discovery-server [CONFIG_XML|SNAPSHOT_XML+]" << std::endl;
    }

    int return_code = 0;
    if (argc == 2)
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
            }
        }
    }
    else
    {
        std::set<std::string> files;
        for (int i = 1; i < argc; ++i)
        {
            files.emplace(argv[i]);
        }
        DSManager manager(files);

        // Check the snapshots read
        if (!manager.validateAllSnapshots())
        {
            LOG_ERROR("Discovery Server error: several snapshots show info leakage");
            return_code = -1; // report CTest the test fail
        }
    }

    Log::Reset();
    Domain::stopAll();

    return return_code;

}
