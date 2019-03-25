
#include "log/DSLog.h"
#include "version/config.h"

#include <fastrtps/log/Log.h>
#include <fastrtps/Domain.h>

#include "DSManager.h"

using namespace eprosima;
using namespace fastrtps;
using namespace rtps;

int main(int argc, char * argv[])
{
    if (!(argc > 1))
    {
        std::cout << "Usage: discovery-server CONFIG_XML" << std::endl;
    }

    std::string path_to_config = argv[1];

    DSManager manager(path_to_config);

    if (manager.isActive())
    {
        std::cout << "\n### Discovery Server is running, press any key to quit ###" << std::endl;
        fflush(stdout);
        std::cin.ignore();
    }
    else
    {
        std::cout << "Discovery Server error: no active servers" << std::endl;
    }

    return 0;

    //Domain::loadXMLProfilesFile("C:\\Users\\MiguelBarro\\OneDrive\\eProsima\\Works\\DiscoveryServer\\profile demos\\Profile_testing\\sample_profile.xml");
    //auto client = Domain::createParticipant("testing client");
    //auto server = Domain::createParticipant("testing server");

    //// here would be some RTPS pub sub operations

    //Domain::removeParticipant(client);
    //Domain::removeParticipant(server);

    //Domain::stopAll();
    //Log::Reset();
    //return 0;
}
