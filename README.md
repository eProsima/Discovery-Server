# eProsima Discovery Server

The [RTPS standard](http://www.omg.org/spec/DDSI-RTPS/2.3) specifies in section 8.5 a non-centralized, distributed,
simple discovery mechanism. This mechanism was devised to allow interoperability between independent
vendor-specific implementations but it is not expected to be optimal in every environment.
There are several scenarios where the simple discovery mechanism is unsuitable, or it just simply cannot be
applied:

- A high number of endpoint entities are continuously entering and exiting a large network.
- Networks without multicasting capabilities.
- WiFi-based communication networks.

In order to cope with the aforementioned issues, the *eProsima Fast DDS* discovery mechanisms are extended with a new
Discovery Server discovery paradigm.
This is based on a client-server architecture, i.e. the metatraffic (message exchange among
DDS DomainParticipants to identify each other) is managed by one or several server DomainParticipants (right figure), as
opposed to simple discovery (left figure), where metatraffic is exchanged using a message broadcast mechanism like an
IP multicast protocol.
Please, refer to
[Fast DDS documentation](https://fast-dds.docs.eprosima.com/en/latest/fastdds/discovery/server_client.html) for
further information about the Discovery Server discovery mechanism.

![discovery diagrams][diagrams]

[diagrams]: resources/images/ds_uml.png

The Discovery Server tool is developed to ease Discovery Server discovery mechanism setup and testing.
The **Discovery Server tool documentation** can be found
[here](https://eprosima-discovery-server.readthedocs.io/en/latest/).

-   [Supported platforms](#supported-platforms)
-   [Installation Guide](#installation-guide)
-   [Usage](#usage)
-   [Example application](#example-application)
-   [Testing](#testing)
-   [Documentation](#documentation)
-   [Getting Help](#getting-help)

## Supported platforms

* Linux
* Windows


## Installation Guide

In order to use the Discovery Server tool, it is necessary to have a compatible version of
[eProsima Fast DDS](https://eprosima-fast-rtps.readthedocs.io/en/latest/) installed (over release 2.0.2).
*Fast DDS* dependencies as tinyxml must be accessible, either because *Fast DDS* was build-installed defining
`THIRDPARTY=ON|FORCE` or because those libraries have been specifically installed.
The well known cross-platform tool [colcon](https://colcon.readthedocs.io/en/released/) was chosen to simplify the
installation of the several mutually dependent [CMake](https://cmake.org/cmake/help/latest/) projects. In order to use
colcon, [Python3](https://www.python.org/) and [CMake](https://cmake.org/cmake/help/latest/) must be first installed.
Detailed instructions on how to install the required dependencies can be found in the
[Discovery Server documentation](https://eprosima-discovery-server.readthedocs.io/en/latest/).

To execute the tests that verify the proper operation of the Discovery Server discovery mechanism, it is necessary
to install some Python3 modules. These can be installed using `pip`.

```bash
pip3 install jsondiff==1.2.0 xmltodict==0.12.0 pandas==1.1.2
```

A [discovery-server.repos](https://raw.githubusercontent.com/eProsima/Discovery-Server/master/discovery-server.repos)
file is available in order to profit from [vcstool](https://github.com/dirk-thomas/vcstool) capabilities to download
the needed repositories.

### Linux

1.  Create a Discovery Server workspace and download the repos file that will be used to install the Discovery Server
    tool and its dependencies:

    ```bash
    $ mkdir -p discovery-server-ws/src && cd discovery-server-ws
    $ wget https://raw.githubusercontent.com/eProsima/Discovery-Server/master/discovery-server.repos
    $ vcs import src < discovery-server.repos
    ```

1.  Finally, use colcon to compile all software.
    Choose the build configuration by declaring ``CMAKE_BUILD_TYPE`` as Debug or Release.
    For this example, the Debug option has been chosen, which would be the choice of advanced users for debugging purposes.

    ```bash
    $ colcon build --base-paths src
            \ --packages-up-to discovery-server
            \ --cmake-args -DTHIRDPARTY=ON -DLOG_LEVEL_INFO=ON -DCOMPILE_EXAMPLES=ON -DINTERNALDEBUG=ON -DCMAKE_BUILD_TYPE=Debug
    ```

1.  If you installed the Discovery Server tool following the steps outlined above, you can try the
    *HelloWorldExampleDS*.
    To run the example navigate to the following directory

    ``<path/to/discovery-server-ws>/discovery-server-ws/install/discovery-server/examples/HelloWorldExampleDS``

    and run

    ```bash
    $ ./HelloWorldExampleDS-d --help
    ```

    to display the example usage instructions.

    In order to test the *HelloWorldExampleDS* open three terminals and run the above command.
    Then run the following command in each terminal:
    -   Terminal 1:
        ```bash
        $ cd <path/to/discovery-server-ws>/discovery-server-ws/install/discovery-server/examples/HelloWorldExampleDS
        $ ./HelloWorldExampleDS publisher
        ```
    -   Terminal 2:
        ```bash
        $ cd <path/to/discovery-server-ws>/discovery-server-ws/install/discovery-server/examples/HelloWorldExampleDS
        $ ./HelloWorldExampleDS subscriber
        ```
    -   Terminal 3:
        ```bash
        $ cd <path/to/discovery-server-ws>/discovery-server-ws/install/discovery-server/examples/HelloWorldExampleDS
        $ ./HelloWorldExampleDS server
        ```

### Windows

1.  Create a Discovery Server workspace and download the repos file that will be used to install the Discovery Server
    tool and its dependencies:

    ```bat
    > mkdir discovery-server-ws
    > cd discovery-server-ws
    > mkdir src
    > wget https://raw.githubusercontent.com/eProsima/Discovery-Server/master/discovery-server.repos
    > vcs import src < discovery-server.repos
    ```

1.  If the generator (compiler) of choice is Visual Studio, launch colcon from a visual studio console.
    Any console can be setup into a visual studio one by executing a batch file.
    For example, in VS2017 is usually ``C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\Tools\VsDevCmd.bat``

1.  Finally, use colcon to compile all software.
    Choose the build configuration by declaring ``CMAKE_BUILD_TYPE`` as Debug or Release.
    For this example, the Debug option has been chosen, which would be the choice of advanced users for debugging purposes.
    If using a multi-configuration generator like Visual Studio we recommend to build both in debug and release modes

    ```bat
    > colcon build --base-paths src
            \ --packages-up-to discovery-server
            \ --cmake-args -DTHIRDPARTY=ON -DLOG_LEVEL_INFO=ON -DCOMPILE_EXAMPLES=ON -DINTERNALDEBUG=ON -DCMAKE_BUILD_TYPE=Debug
    > colcon build --base-paths src
            \ --packages-up-to discovery-server
            \ --cmake-args -DTHIRDPARTY=ON -DCOMPILE_EXAMPLES=ON -DCMAKE_BUILD_TYPE=Release
    ```

1. If you installed the Discovery Server tool following the steps outlined above, you can try the
    *HelloWorldExampleDS*.
    To run the example navigate to the following directory

    ``<path/to/discovery-server-ws>/discovery-server-ws/install/discovery-server/examples/HelloWorldExampleDS``

    and run

    ```bash
    > HelloWorldExampleDS --help
    ```

    to display the example usage instructions.

    In order to test the *HelloWorldExampleDS* open three terminals and run the above command.
    Then run the following command in each terminal:
    -   Terminal 1:
        ```bash
        > cd <path/to/discovery-server-ws>/discovery-server-ws/install/discovery-server/examples/HelloWorldExampleDS
        > HelloWorldExampleDS publisher
        ```
    -   Terminal 2:
        ```bash
        > cd <path/to/discovery-server-ws>/discovery-server-ws/install/discovery-server/examples/HelloWorldExampleDS
        > HelloWorldExampleDS subscriber
        ```
    -   Terminal 3:
        ```bash
        > cd <path/to/discovery-server-ws>/discovery-server-ws/install/discovery-server/examples/HelloWorldExampleDS
        > HelloWorldExampleDS server

---
**NOTE**

In order to avoid using *vcstool*, the following repositories should be downloaded from github into
the `discovery-server-ws/src` directory:

|            PACKAGE                 |                              URL                        |    BRANCH   |
|:-----------------------------------|:--------------------------------------------------------|:-----------:|
| eProsima/Fast-CDR:                 | https://github.com/eProsima/Fast-CDR.git                |    master   |
| eProsima/Fast-RTPS:                | https://github.com/eProsima/Fast-RTPS.git               |    master   |
| eProsima/Discovery-Server:         | https://github.com/eProsima/Discovery-Server.git        |    master   |
| eProsima/foonathan_memory_vendor:  | https://github.com/eProsima/foonathan_memory_vendor.git |    master   |
| leethomason/tinyxml2:              | https://github.com/leethomason/tinyxml2.git             |    master   |

---

## Usage

### XML profiles

Each setting in *Fast DDS* can be configured through XML profiles. XML profiles allows to avoid tiresome
hard-coded settings within applications sources using XML configuration files.
The *Fast DDS* XML schema was duly updated to accommodate the new Discovery Server tool settings.

-   The participant profile ``<builtin>`` tag contains a ``<discovery_config>`` tag where all discovery-related info is
    gathered. This new tag contains the following new XML child elements:
	- ``<discoveryProtocol>``: specifies the discovery type through the ``DiscoveryProtocol_t`` enumeration.
	- ``<discoveryServersList>``: specifies the server or servers to which a Client/Server connects.
	- ``<clientAnnouncementPeriod>``: specifies the time span between PDP metatraffic exchange.

An example of XML profiles for a client and a server is shown below.

```xml
<participant profile_name="UDP_client" >
  <rtps>
	<builtin>
		<discovery_config>
		  <discoveryProtocol>CLIENT</discoveryProtocol>
		  <discoveryServersList>
			<RemoteServer prefix="4d.49.47.55.45.4c.5f.42.41.52.52.4f">
			  <metatrafficUnicastLocatorList>
				<locator>
				  <udpv4>
					<address>192.168.1.113</address>
					<port>64863</port>
				  </udpv4>
				</locator>
			  </metatrafficUnicastLocatorList>
			</RemoteServer>
		  </discoveryServersList>
		</discovery_config>
	</builtin>
  </rtps>
</participant>

<participant profile_name="UDP_server">
  <rtps>
	<prefix>4d.49.47.55.45.4c.5f.42.41.52.52.4f</prefix>
	<builtin>
		<discovery_config>
		  <discoveryProtocol>SERVER</discoveryProtocol>
		</discovery_config>
		<metatrafficUnicastLocatorList>
			<locator>
				<udpv4>
					<address>192.168.1.113</address>
					<port>64863</port>
				</udpv4>
			</locator>
		</metatrafficUnicastLocatorList>
	</builtin>
  </rtps>
</participant>
```

### Discovery Server executable

The discovery server binary (named after the pattern ``discovery-server-X.X.X(d)`` where ``X.X.X`` is the version
number and the optional *d* denotes a debug build) is set up from one XML profiles files passed as command-line
arguments.
To have the tool accessible in the terminal session it is necessary to source the setup file.

-   Linux
    ```bash
    $ source <path/to/discovery-server-ws>/discovery-server-ws/install/setup.bash
    $ discovery-server-X.X.X(d) -c config_file.xml
    ```

-   Windows

    ```bat
    > . <path/to/discovery-server-ws>/discovery-server-ws/install/setup.bat
    > discovery-server-X.X.X(d).exe config_file.xml
    ```

## Documentation

You can access the documentation online, which is hosted on
[Read the Docs](https://eprosima-discovery-server.readthedocs.io/en/latest/).

<!-- * [Start Page](https://eprosima-discovery-server.readthedocs.io/en/latest/)
* [Installation manual](https://eprosima-discovery-server.readthedocs.io/en/latest/installation.html)
* [User manual](https://eprosima-discovery-server.readthedocs.io/en/latest/command_line.html)
* [Examples](https://eprosima-discovery-server.readthedocs.io/en/latest/HelloWorldExample.html)
* [Tests](https://eprosima-discovery-server.readthedocs.io/en/latest/tests.html)
* [Release notes](https://eprosima-discovery-server.readthedocs.io/en/latest/notes.html) -->



## Getting Help

If you need support you can reach us by mail at `support@eProsima.com` or by phone at `+34 91 804 34 48`.

