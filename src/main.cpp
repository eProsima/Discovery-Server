
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
    #ifdef LOG_LEVEL_INFO
        Log::SetVerbosity(Log::Kind::Info);
    #elif LOG_LEVEL_WARN
        Log::SetVerbosity(Log::Kind::Warning);
    #elif LOG_LEVEL_ERROR
        Log::SetVerbosity(Log::Kind::Error);
    #endif

    if (!(argc > 1))
    {
        std::cout << "Usage: discovery-server CONFIG_XML" << std::endl;
    }

    std::string path_to_config = argv[1];
    
    {
        DSManager manager(path_to_config);

        if (manager.isActive())
        {
            manager.runEvents();

            //std::cout << "\n### Discovery Server is running, press any key to quit ###" << std::endl;
            //fflush(stdout);
            //std::cin.ignore();

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
