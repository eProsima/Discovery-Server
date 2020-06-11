# Copyright 2020 Proyectos y Sistemas de Mantenimiento SL (eProsima).
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
fastserver_path = sys.argv[1]           # '\\...\\discovery-server\\build64\\Debug\\fast-discovery-serverd-1.0.0'
server_listening_port = sys.argv[2]     # CMake generated random port
discovery_path = sys.argv[3]            # '\\...\\discovery-server\\build64\\Debug\\discovery-server-1.1.0d.exe'
xml_config_path = sys.argv[4]           # '\\...\\discovery-server\\build64\\Debug\\test_15_fds.xml'

# print(f'arguments: {fastserver_path} {server_listening_port} {discovery_path} {xml_config_path}')

# launch server and clients
server = subprocess.Popen([fastserver_path, '-i',  '5', '-l', '127.0.0.1', '-p', f'{server_listening_port}'])
clients = subprocess.Popen([discovery_path, xml_config_path])

# wait till clients test is finished, then kill the server 
clients.communicate()
server.kill()

if clients.returncode: # 0 if everything goes fine, validation passed
    print(f'discovery-server process fault on clients: returncode {clients.returncode}', file=sys.stderr) 
    sys.exit(clients.returncode)

# success
sys.exit(0)
