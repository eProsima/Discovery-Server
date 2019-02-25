// Copyright 2019 Proyectos y Sistemas de Mantenimiento SL (eProsima).
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

/**
 * @file DServerEvent.h
 *
 */

#ifndef DSERVEREVENT_H_
#define DSERVEREVENT_H_
#ifndef DOXYGEN_SHOULD_SKIP_THIS_PUBLIC

#include <fastrtps\rtps\resources\TimedEvent.h>

namespace eprosima {
namespace fastrtps{
namespace rtps {

class PDPServer;

/**
 * Class DSClientEvent, TimedEvent used to synchronize discover-server clients
 *@ingroup DISCOVERY_MODULE
 */
class DServerEvent : public TimedEvent {
public:

	/**
	 * Constructor.
	 * @param p_PDP Pointer to the PDPServer.
	 * @param interval Interval in ms.
	 */
    DServerEvent(PDPServer* p_PDP,
			double interval);
	~DServerEvent();
	
	/**
	* Method invoked when the event occurs.
	* This temporal event:
        + resends the client RTPSParticipantProxyData to all remote servers.
        + matches the EDP endpoints when the servers are all aware of this client existence
	* @param code Code representing the status of the event
	* @param msg Message associated to the event
	*/
	void event(EventCode code, const char* msg= nullptr);
	
	//!Pointer to the PDPServer object.
    PDPServer* mp_PDP;
};
}
} /* namespace rtps */
} /* namespace eprosima */
#endif
#endif /* DSERVEREVENT_H_ */
