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

import subprocess, time, os, sys, tempfile, xml.etree.ElementTree as ET 

# compose the arguments
discovery_path = sys.argv[1]     # '\\...\\discovery-server\\build64\\Debug\\discovery-server-1.1.0d.exe'
xml_server_path = sys.argv[2]     # '\\...\\discovery-server\\resources\\xml\\test_08_lease_server.xml'
xml_client_path = sys.argv[3]     # '\\...\\discovery-server\\resources\\xml\\test_08_lease_client.xml'

# print(f'arguments: {discovery_path} {xml_server_path} {xml_client_path}')

tmpdir = tempfile.mkdtemp()
# print(f'temporary dir: {tmpdir}')

# launch 
proc_server = subprocess.Popen([discovery_path, xml_server_path], cwd=tmpdir)
proc_client = subprocess.Popen([discovery_path, xml_client_path], cwd=tmpdir)

# wait 3 seconds before killing the client
try:
    proc_client.wait(3)
except subprocess.TimeoutExpired:
    proc_client.kill()

# wait for server completion
proc_server.communicate()

if proc_server.returncode: # 0 if everything goes fine
    print(f'discovery-server process fault: returncode {proc_server.returncode}', file=sys.stderr) 
    sys.exit(proc_server.returncode)

# Verify if the file is there
snapshot = os.path.join(tmpdir,'test_8_server_snapshot.xml')
#print(f'Output snapshot file: {snapshot}')

if not os.path.exists(snapshot):
    print('snapshot file was not generated.', file=sys.stderr)
    sys.exit(1)

# load the file
tree = ET.parse(snapshot)

# query the number of times clien3 appears on the first snapshot. It should be 3 (one for each participant in the server
# config file) 
nodes1 = tree.findall('./DS_Snapshot[1]//*[@name="client3"]')

# query the number of times clien3 appears on the second snapshot. It should be none because the server should notify
# all clients of client3 demise 
nodes2 = tree.findall('./DS_Snapshot[2]//*[@name="client3"]')

if not(len(nodes1) == 3 and len(nodes2) == 0): 
    print('Snapshots show that lease duration is not working properly', file=sys.stderr)
    sys.exit(2)

# verify all participants on the server side share the same info
proc_val = subprocess.run([discovery_path, snapshot])

if proc_val.returncode: # 0 if everything goes fine
    print(f'discovery info validation fails: returncode {proc_val.returncode}', file=sys.stderr) 
    sys.exit(proc_val.returncode)

# clean things up
try:
    os.remove(snapshot)
    os.rmdir(tmpdir)
except:
    print('Unable to remove the temporary files and dirs generated in the test.', file=sys.stderr)
    sys.exit(3)

sys.exit(proc_server.returncode) # 0 if everything goes fine
