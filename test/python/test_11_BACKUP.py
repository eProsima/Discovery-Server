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

import subprocess, time, os, sys, tempfile, shutil

# compose the arguments
discovery_path = sys.argv[1]     # '\\...\\discovery-server\\build64\\Debug\\discovery-server-1.1.0d.exe'
xml_server_path = sys.argv[2]     # '\\...\\discovery-server\\resources\\xml\\'test_11_BACKUP_server.xml'
xml_client_path = sys.argv[3]     # '\\...\\discovery-server\\resources\\xml\\'test_11_BACKUP_client.xml'

# print(f'arguments: {discovery_path} {xml_server_path} {xml_client_path}')

tmpdir = tempfile.mkdtemp()
# print(f'temporary dir: {tmpdir}')

# launch server and clients, the current directory is the temporary one. The backup file will be there. 
proc_server = subprocess.Popen([discovery_path, xml_server_path], cwd=tmpdir)
proc_client = subprocess.Popen([discovery_path, xml_client_path], cwd=tmpdir) 

# wait 3 seconds before killing the server, time enought to have all client info recorded in the backup file 
try:
    proc_server.wait(3)
except subprocess.TimeoutExpired:
    proc_server.kill()

# now we relaunch the server again and expect him to reload the data from the clients
result = subprocess.run([discovery_path, xml_server_path], cwd=tmpdir)

if result.returncode: # 0 if everything goes fine 
    print(f'failure when running {xml_server_path}')
    print(result.stderr, file=sys.stderr)
    sys.exit(result.returncode)

# wait for client completion
proc_client.communicate()

if proc_client.returncode: # 0 if everything goes fine
    print(f'failure when running {xml_client_path}')
    sys.exit(proc_client.returncode)

# Verify the snapshot files are there
snapshot_server = os.path.join(tmpdir,'test_11_server_snapshot.xml')
snapshot_client = os.path.join(tmpdir,'test_11_client_snapshot.xml')

if not (os.path.exists(snapshot_server) and os.path.exists(snapshot_client)):
    print('snapshot files were not generated.', file=sys.stderr)
    sys.exit(1)

# Validate the snapshots
result = subprocess.run([discovery_path, snapshot_server, snapshot_client], cwd=tmpdir)

if result.returncode: # 0 if everything goes fine 
    print(f'snapshot files validation failed. Files are located in {tmpdir}', file=sys.stderr)
    print(result.stdout, file=sys.stderr)
    sys.exit(result.returncode)

# clean things up
try:
    os.remove(snapshot_server)
    os.remove(snapshot_client)
    shutil.rmtree(tmpdir)
except:
    print(f'Unable to remove the temporary files and dirs generated in the test. Files are located in {tmpdir}',
        file=sys.stderr)
    sys.exit(2)

# success
sys.exit(0)
