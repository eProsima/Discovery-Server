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

import subprocess, time, os, sys, tempfile, shutil

# compose the arguments
discovery_path = sys.argv[1]     # '\\...\\discovery-server\\build64\\Debug\\discovery-server-1.2.0d.exe'
xml_server_path = sys.argv[2]     # '\\...\\discovery-server\\build64\\'test_12_interprocess_server.xml'
xml_client1_path = sys.argv[3]     # '\\...\\discovery-server\\build64\\'test_12_interprocess_client1.xml'
xml_client2_path = sys.argv[4]     # '\\...\\discovery-server\\build64\\'test_12_interprocess_client2.xml'
xml_client3_path = sys.argv[5]     # '\\...\\discovery-server\\build64\\'test_12_interprocess_client3.xml'

# print(f'arguments: {discovery_path} {xml_server_path} {xml_client1_path} {xml_client2_path} {xml_client3_path}')

tmpdir = tempfile.mkdtemp()
# print(f'temporary dir: {tmpdir}')

# launch server and clients in different processes to simulate different machiens,
# the current directory is the temporary one. 
proc_server = subprocess.Popen([discovery_path, xml_server_path], cwd=tmpdir)
proc_client1 = subprocess.Popen([discovery_path, xml_client1_path], cwd=tmpdir) 
proc_client2 = subprocess.Popen([discovery_path, xml_client2_path], cwd=tmpdir) 
proc_client3 = subprocess.Popen([discovery_path, xml_client3_path], cwd=tmpdir) 

# wait for server completion
proc_server.communicate()
proc_client1.communicate()
proc_client2.communicate()
proc_client3.communicate()

if proc_server.returncode: # 0 if everything goes fine
    print(f'discovery-server process fault on server: returncode {proc_server.returncode}', file=sys.stderr) 
    sys.exit(proc_server.returncode)

if proc_client1.returncode: # 0 if everything goes fine
    print(f'discovery-server process fault on client1: returncode {proc_client1.returncode}', file=sys.stderr) 
    sys.exit(proc_client1.returncode)

if proc_client2.returncode: # 0 if everything goes fine
    print(f'discovery-server process fault on client2: returncode {proc_client2.returncode}', file=sys.stderr) 
    sys.exit(proc_client2.returncode)

if proc_client3.returncode: # 0 if everything goes fine
    print(f'discovery-server process fault on client3: returncode {proc_client3.returncode}', file=sys.stderr) 
    sys.exit(proc_client3.returncode)

# Verify the snapshot files are there
snapshot_server = os.path.join(tmpdir,'snapshot_server.xml')
snapshot_client1 = os.path.join(tmpdir,'snapshot_client1.xml')
snapshot_client2 = os.path.join(tmpdir,'snapshot_client2.xml')
snapshot_client3 = os.path.join(tmpdir,'snapshot_client3.xml')

if not (os.path.exists(snapshot_server) and os.path.exists(snapshot_client1)
   and os.path.exists(snapshot_client2) and os.path.exists(snapshot_client3)):
    print('snapshot files were not generated.', file=sys.stderr)
    sys.exit(1)

# Validate the snapshots
result = subprocess.run([discovery_path, snapshot_server, snapshot_client1,
    snapshot_client2, snapshot_client3], cwd=tmpdir)

if result.returncode: # 0 if everything goes fine 
    print(f'snapshot files validation failed. Files are located in {tmpdir}', file=sys.stderr)
    print(result.stdout, file=sys.stderr)
    sys.exit(result.returncode)

# Generate a common snapshot file and revalidate
snapshot = os.path.join(tmpdir,'snapshot.xml')
result = subprocess.run([discovery_path, snapshot_server, snapshot_client1,
    snapshot_client2, snapshot_client3, '-out', snapshot], cwd=tmpdir)

if result.returncode: # 0 if everything goes fine 
    print(f'snapshots aggregation failed. Files are located in {tmpdir}', file=sys.stderr)
    print(result.stdout, file=sys.stderr)
    sys.exit(result.returncode)

if not (os.path.exists(snapshot)):
    print('Aggregated snapshot file was not generated.', file=sys.stderr)
    sys.exit(1)

result = subprocess.run([discovery_path, snapshot], cwd=tmpdir)

if result.returncode: # 0 if everything goes fine 
    print(f'Aggregated snapshot validation failed. Files are located in {tmpdir}', file=sys.stderr)
    print(result.stdout, file=sys.stderr)
    sys.exit(result.returncode)

# clean things up
try:
    os.remove(snapshot_server)
    os.remove(snapshot_client1)
    os.remove(snapshot_client2)
    os.remove(snapshot_client3)
    os.remove(snapshot)
    shutil.rmtree(tmpdir)
except:
    print(f'Unable to remove the temporary files and dirs generated in the test. Files are located in {tmpdir}',
        file=sys.stderr)
    sys.exit(2)

# success
sys.exit(0)
