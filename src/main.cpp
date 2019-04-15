
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
    #if LOG_LEVEL_INFO
        Log::SetVerbosity(Log::Kind::Info);
    #elif LOG_LEVEL_WARN
        Log::SetVerbosity(Log::Kind::Warning);
    #elif LOG_LEVEL_ERROR
        Log::SetVerbosity(Log::Kind::Error);
    #endif

    Log::SetCategoryFilter(std::regex("(DISCOVERY_SERVER)|(SERVER_PDP_THREAD)|(CLIENT_PDP_THREAD)"));
    // Log::SetCategoryFilter(std::regex("(SERVER_PDP_THREAD)|(CLIENT_PDP_THREAD)"));

    if (!(argc > 1))
    {
        std::cout << "Usage: discovery-server CONFIG_XML" << std::endl;
    }

    std::string path_to_config = argv[1];
    
    {
        DSManager manager(path_to_config);

        if (manager.isActive())
        {
            // Follow the config file instructions
            manager.runEvents(std::cin, std::cout);
            // Check the snapshots taken
            if (!manager.validateAllSnapshots())
                std::cout << "Discovery Server error: several snapshots show info leakage";
        }
        else
        {
            std::cout << "Discovery Server error: no active servers" << std::endl;
        }

    }
    
    Domain::stopAll();
    Log::Reset();

    return 0;

}
