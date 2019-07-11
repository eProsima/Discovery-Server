# Copyright 2019 Proyectos y Sistemas de Mantenimiento SL (eProsima).
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

# min lowered admissible port
# max higher admissible port
function( random_port min max output_variable)

	set(PORT_NUMBER 0)

	while (NOT PORT_NUMBER)
		string(RANDOM LENGTH 5 ALPHABET 0123456789 PORT_NUMBER)
		
		if( PORT_NUMBER LESS_EQUAL min OR PORT_NUMBER GREATER_EQUAL max)
			set(PORT_NUMBER 0)
		endif()
	
	endwhile()
	
	set( ${output_variable} ${PORT_NUMBER} PARENT_SCOPE)
	
endfunction()