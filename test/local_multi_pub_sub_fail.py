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

import sys, os, subprocess, glob

script_dir = os.path.dirname(os.path.realpath(__file__))

ds_command = os.environ.get("DISCOVERY_SERVER_BIN")
if not ds_command:
    rc = subprocess.call(['which', "discovery-server"])
    if rc == 0:
        ds_command = "discovery-server"
if not ds_command:
    ds_files = glob.glob(os.path.join(script_dir, "**/discovery-server*"), recursive=True)
    ds_command = next(iter(ds_files), None)
assert ds_command

# Launch processes
print("Launching server and both clients")
server = subprocess.Popen([ds_command, "test_3_server.xml"])
client1 = subprocess.Popen([ds_command, "test_3_1.xml"])
client2 = subprocess.Popen([ds_command, "test_3_2.xml"])

# Wait until finish
print("Waiting them to finish")
server.wait()
client1.wait()
client2.wait()

# Launch check process
print("Checking results")
check = subprocess.Popen([ds_command, "snapshots_3_client1.xml", "snapshots_3_client2.xml", "snapshots_3_server.xml"])

check.wait()
retvalue = check.returncode

if retvalue == 0:
    print("Test failed")
    sys.exit(-1)
else:
    print("Test successed")
    sys.exit(0)
