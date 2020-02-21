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
xml_server_path = sys.argv[2]     # '\\...\\discovery-server\\resources\\xml\\test_9_WLP_server.xml'
xml_client_path = sys.argv[3]     # '\\...\\discovery-server\\resources\\xml\\test_9_WLP_client.xml'

# print(f'arguments: {discovery_path} {xml_server_path} {xml_client_path}')

tmpdir = tempfile.mkdtemp()
# print(f'temporary dir: {tmpdir}')

# launch 
proc_server = subprocess.Popen([discovery_path, xml_server_path], cwd=tmpdir)
proc_client = subprocess.Popen([discovery_path, xml_client_path], cwd=tmpdir) 

# wait 4 seconds before killing the client
try:
    proc_client.wait(4)
except subprocess.TimeoutExpired:
    proc_client.kill()

# wait for server completion
proc_server.communicate()

if proc_server.returncode: # 0 if everything goes fine
    print(f'discovery-server process fault: returncode {proc_server.returncode}', file=sys.stderr) 
    sys.exit(proc_server.returncode)

# Verify if the file is there
snapshot = os.path.join(tmpdir,'test_9_server_snapshot.xml')
#print(f'Output snapshot file: {snapshot}')

if not os.path.exists(snapshot):
    print('snapshot file was not generated.', file=sys.stderr)
    sys.exit(1)

# load the file
tree = ET.parse('C:\\Users\\MIGUEL~1\\AppData\\Local\\Temp\\tmpi6nw2yie\\test_9_server_snapshot.xml')

# query the alive_count attributes on the first DS_Snapshot to verify all subscribers detected the publisher 
nodes1 = tree.findall('./DS_Snapshot[1]//subscriber[@alive_count="1"]') 

# query the alive_count attributes on the second DS_Snapshot to verify all subscriber notice the publisher dead
nodes2 = tree.findall('./DS_Snapshot[2]//subscriber[@alive_count="0"]') 

if not(len(nodes1) == len(nodes2) == 2): 
    print('Snapshots show the liveliness is not working properly', file=sys.stderr)
    sys.exit(2)

# clean things up
try:
    os.remove(snapshot)
    os.rmdir(tmpdir)
except:
    print('Unable to remove the temporary files and dirs generated in the test.', file=sys.stderr)
    sys.exit(3)

# success
sys.exit(0)
