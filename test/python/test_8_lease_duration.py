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

import subprocess, time, os, sys

# compose the arguments
discovery_path = sys.argv[1]     # '\\...\\discovery-server\\build64\\Debug\\discovery-server-1.1.0d.exe'
xml_server_path = sys.argv[2]     # '\\...\\discovery-server\\resources\\xml\\test_8_lease_server.xml'
xml_client_path = sys.argv[3]     # '\\...\\discovery-server\\resources\\xml\\test_8_lease_client.xml'

# launch 
proc_server = subprocess.Popen([discovery_path, xml_server_path])
proc_client = subprocess.Popen([discovery_path, xml_client_path])

# wait 3 seconds before killing the client
try:
    proc_client.wait(3)
except subprocess.TimeoutExpired:
    proc_client.kill()

# wait for server completion
proc_server.communicate()

sys.exit(proc_server.returncode) # 0 if everything goes fine
