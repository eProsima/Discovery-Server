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

import subprocess, time, os, sys, tempfile, shutil, sqlite3

# compose the arguments
discovery_path = sys.argv[1]     # '\\...\\discovery-server\\build64\\Debug\\discovery-server-1.1.0d.exe'
xml_config_server_path = sys.argv[2]     # '\\...\\discovery-server\\build64\\Debug\\test_13_trimming_server.xml'
xml_config_client_path = sys.argv[3]     # '\\...\\discovery-server\\build64\\Debug\\test_13_trimming_client.xml'

# print(f'arguments: {discovery_path} {xml_config_path}')

tmpdir = tempfile.mkdtemp()
# print(f'temporary dir: {tmpdir}')

# launch server and clients, the current directory is the temporary one. The backup file will be there.
proc_server = subprocess.Popen([discovery_path, xml_config_server_path], cwd=tmpdir)
proc_client = subprocess.Popen([discovery_path, xml_config_client_path], cwd=tmpdir)

# wait 3 seconds before killing the remote client, because lease duration has been reduced to 6s the drop will take
# place 9 seconds after the execution started
try:
    proc_client.wait(3)
except subprocess.TimeoutExpired:
    proc_client.kill()

# wait 9 seconds before killing the server, that is, 12s from execution startup. It's time enought for the trimming to
# be done and all client info recorded in the backup file
try:
    proc_server.wait(9)
except subprocess.TimeoutExpired:
    proc_server.kill()

# check the backup file is in place
backup_file = os.path.join(tmpdir,'server-4d-49-47-55-45-4c-5f-42-41-52-52-4f.db')

if not (os.path.exists(backup_file)):
    print('Backup file was not generated.', file=sys.stderr)
    sys.exit(1)

# Open the database
con = sqlite3.connect(backup_file)

# validate the contents
# number of PDP entries must be 2 (client2 + server)
pn = con.execute("select count(*) from writers where guid like '%0.1.0.c2'").fetchone()[0]
rn = con.execute("select count(*) from writers where guid like '%0.0.4.c2'").fetchone()[0]
wn = con.execute("select count(*) from writers where guid like '%0.0.3.c2'").fetchone()[0]

# print(f'Number of DATA(p) = {pn}, DATA(r) = {rn}, DATA(w) = {wn} in backup database')

if 2 != pn:
    print(f'Database file validation failed. PDP trim failed. {pn} participants registered.'
          f' Files are located in {tmpdir}', file=sys.stderr)
    sys.exit('PDP trim failure.')

# only one EDP subscriber entries (client2)
if 1 != rn:
    print(f'Database file validation failed. EDP subscriber trim failed. {rn} subscribers registered.'
          f'Files are located in {tmpdir}', file=sys.stderr)
    sys.exit('EDP subscriber trim failure.')

# only one EDP publisher entries (client2)
if 1 != wn:
    print(f'Database file validation failed. EDP publisher trim failed. {wn} publishers registered.'
          f'Files are located in {tmpdir}', file=sys.stderr)
    sys.exit('EDP publisher trim failure.')

# we are done with the database
con.close()

# clean things up
try:
    os.remove(backup_file)
    shutil.rmtree(tmpdir)
except:
    print(f'Unable to remove the temporary files and dirs generated in the test. Files are located in {tmpdir}',
        file=sys.stderr)
    sys.exit(2)

# success
sys.exit(0)
