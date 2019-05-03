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

import sys, os, subprocess, glob, argparse, pexpect
from pexpect import pxssh

script_dir = os.path.dirname(os.path.realpath(__file__))

class Host:
    def __init__(self):
        self.host = ""
        self.user = ""
        self.password = ""
        self.testfile = ""

# @BARRO Example command:
# > python3 remote_multi.py --hosts raspa raspe raspi raspo raspu --users test --passwords test
# --testfiles rasp[aeiou]_test.xml

parser = argparse.ArgumentParser()
parser.add_argument("-ds", "--discoveryserver", help="discovery-server executable")
group = parser.add_argument_group("hosts", "list of remote machines where launch the test.")
group.add_argument("--hosts", help="List of Hosts", required=True, action="append", nargs='+')
group.add_argument("--users", help="List of User to login into each Host. Can be only one if all hosts share it.",
                    required=True, action="append", nargs='+')
group.add_argument("--passwords",
                    help="List of Password to login as User into each Host.  Can be only one if all hosts share it.",
                    required=True, action="append", nargs='+')
group.add_argument("--testfiles", help="List of Test files to execute in each Host", required=True, action="append",
                    nargs='+')
args = parser.parse_args()

hosts=[]
for hs in args.hosts:
    for h in hs:
        new_host = Host()
        new_host.host = h
        hosts.append(new_host)

i = 0
for us in args.users:
    if len(us) == 1:
        for h in hosts:
            h.user = us[0]
    else:
        if len(us) != len(hosts):
            print ("ERROR: Number of users mismatch the number of hosts.")
            sys.exit(-1)
        for u in us:
            hosts[i].user = u
            i += 1

i = 0
for ps in args.passwords:
    if len(ps) == 1:
        for h in hosts:
            h.password = ps[0]
    else:
        if len(ps) != len(hosts):
            print ("ERROR: Number of passwords mismatch the number of hosts.")
            sys.exit(-1)
        for p in ps:
            hosts[i].password = p
            i += 1

i = 0
for ts in args.testfiles:
    if len(ts) != len(hosts):
        print ("ERROR: Number of test files mismatch the number of hosts.")
        sys.exit(-1)
    for t in ts:
        hosts[i].testfile = t
        i += 1

# @BARRO DEBUG PRINT
for h in hosts:
    print(h.host)
    print(h.user)
    print(h.password)
    print(h.testfile)
    print("----")



########## @BARRO REMOVE THIS sys.exit after testing to launch the test.
sys.exit(0)
##########

ds_command = args.discoveryserver
if not ds_command:
    ds_command = os.environ.get("DISCOVERY_SERVER_BIN")
if not ds_command:
    rc = subprocess.call(['which', "discovery-server"])
    if rc == 0:
        ds_command = "discovery-server"
if not ds_command:
    ds_files = glob.glob(os.path.join(script_dir, "**/discovery-server*"), recursive=True)
    ds_command = next(iter(ds_files), None)
assert ds_command

# Copy test folder
for h in hosts:
    # Copies current folder to remote host's path
    # copy_proc = subprocess.Popen(["rsync", "-r", ".", h.user+":"+h.password+"@"+h.host+":~"])
    # @BARRO We need to install pexpect! (pip3 install pexpect)
    rsync_cmd = "rsync -r . %s@%s:~" % (h.user, h.host)
    copy_proc = pexpect.spawn(rsync_cmd, timeout=5)
    copy_proc.sendline(h.password)
    copy_proc.expect(pexpect.EOF)
    copy_proc.close()

# Launch remote tests @BARRO using pxssh (pexpect extension)
for h in hosts:
    ssh_proc = pxssh.pxssh()
    ssh_proc.login(h.host, h.user, h.password)
    ssh_proc.sendline(ds_command + " " + h.testfile)
    ssh_proc.prompt()
    ssh_proc.logout()

# Retrieve results... with rsync the results should be already in our test folder (".")

results = []
for h in hosts:
    results.append("out_" + h.testfile)

# Launch check process
print("Checking results")
check = subprocess.Popen([ds_command, results])

check.wait()
retvalue = check.returncode

if retvalue != 0:
    print("Test failed: " + str(retvalue))
else:
    print("Test successed")

sys.exit(retvalue)
