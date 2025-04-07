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


###############################################################################
# Necessary files
###############################################################################
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/configuration DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shared DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/validation DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/run_test.py
        ${CMAKE_CURRENT_BINARY_DIR}/run_test.py COPYONLY)


set(BINARY_TEST_DIR ${CMAKE_CURRENT_BINARY_DIR})
file(TO_NATIVE_PATH "${BINARY_TEST_DIR}/" BINARY_TEST_DIR)

set(BINARY_CONF_DIR ${CMAKE_CURRENT_BINARY_DIR}/configuration)
file(TO_NATIVE_PATH "${BINARY_CONF_DIR}/" BINARY_CONF_DIR)

set(BINARY_TEST_CASES_DIR ${BINARY_TEST_DIR}/test_cases)
file(TO_NATIVE_PATH "${BINARY_TEST_CASES_DIR}/" BINARY_TEST_CASES_DIR)

set(BINARY_SNAPSHOTS_DIR ${BINARY_TEST_DIR}/test_solutions)
file(TO_NATIVE_PATH "${BINARY_SNAPSHOTS_DIR}/" BINARY_SNAPSHOTS_DIR)

set(RUN_TEST ${BINARY_TEST_DIR}run_test.py)
set(TESTS_PARAMS ${BINARY_CONF_DIR}tests_params.json)

# It is possible to create different executables for shm ON and OFF and intraprocess ON and OFF but will
# need nestead loops over different arguments for the same test <run_test.py>
# To save time is adviceable to run ctest -j 10 to parallelized the test execution.
# Note that test doesn't interfere with each other because independent ports are used for the servers.

unset(TEST_LIST)
# These tests run in all Fast DDS 2.x branches
list(APPEND TEST_LIST
        test_00_tool_help
        test_01_trivial
        test_02_single_server_medium
        test_03_single_server_large
        test_04_server_ping
        test_05_server_double_ping
        test_06_diamond_servers
        test_07_server_endpoints_two_servers
        test_08_server_endpoints_four_clients
        test_09_servers_serial
        test_10_server_redundancy
        test_11_remote_servers
        test_12_virtual_topics
        test_13_disposals_single_server
        test_14_disposals_remote_servers
        test_15_disposals_client_servers
        test_16_lease_duration_single_client
        test_17_lease_duration_remove_client_server
        test_18_disposals_remote_servers_multiprocess
        test_19_disposals_break_builtin_connections
        test_20_break_builtin_connections
        test_21_disposals_remote_server_trivial
        test_22_environment_variable_setup

        # test_24_backup
        # test_25_backup_compatibility
        # test_26_backup_restore

        test_27_slow_arise
        test_28_slow_arise_interconnection
        test_29_server_ping_late_joiner
        test_30_connect_locally_with_remote_entity
        test_31_matched_servers_not_share_info
        test_32_superclient_trivial
        test_33_superclient_complex

        test_34_connect_locally_with_remote_server

        test_38_self_connection

        test_39_trivial_reconnect
        test_40_trivial_server_reconnect
        test_41_reconnect_with_clients
        test_42_server_reconnect_with_clients
        test_43_complex_reconnect
        test_44_fast_discovery_server_tool_reconnect
        test_45_trivial_client_reconnect

        test_46_guidless_discovery
        test_47_guidless_server_double_ping
        test_48_guidless_complex

        test_60_disconnection
        test_61_superclient_environment_variable

        test_80_auto
        test_81_auto_ros_domain_id_env_var
        test_82_auto_ros_static_peers_env_var
        test_83_start
        test_84_add
        test_85_set_add_stop
        test_86_stop_all_servers
        test_87_set_modifies_env_var

        test_93_tcp_reconnect_with_clients
        test_94_tcpv4_custom_guid_transform_locators
        test_95_tcpv4_cli
        test_96_tcpv6_cli
        test_97_tcpv4_env_var
        test_98_tcpv6_env_var
        test_99_tcp

        test_101_trivial_xtypes
        test_102_single_server_medium_xtypes
        test_103_single_server_large_xtypes
        test_106_diamond_servers_xtypes
        test_108_server_endpoints_four_clients_xtypes
        test_114_disposals_remote_servers_xtypes
    )

# This test does not run in Fast DDS 2.0.x because signal handling for closing the tool was not
# backported.

    list(APPEND TEST_LIST
    test_23_fast_discovery_server_tool
    )

# Adding remote servers to the server using env var was introduced in Fast DDS v2.3.

    list(APPEND TEST_LIST
        test_35_fds_two_connected_servers_with_clients
    )

# Adding DNS and Environment modification while running was introduced in Fast DDS v2.4.0

    list(APPEND TEST_LIST
        test_36_dns_environment_variable_setup
        test_37_dns_fast_discovery_server_tool

        test_50_environment_modification
    )

# The above TEST_LIST has tests that spawn multiple test cases, we need a new variable to enumerate them
set(TEST_CASE_LIST)

# Test that are leaky and may fail
set(FAIL_TEST_CASES)

# For each test case, create different executables for each configuration
find_program(FASTDDS_PYTHON_EXECUTABLE fastdds PATHS ${CMAKE_INSTALL_PREFIX}/bin)
if(FASTDDS_PYTHON_EXECUTABLE)
    message(STATUS "Found Fast DDS CLI executable at: ${FASTDDS_PYTHON_EXECUTABLE}")
else()
    message(WARNING "Fast DDS CLI Executable not found!")
endif()
foreach(TEST IN LISTS TEST_LIST)

    unset(TEST_NAME)
    set(TEST_NAME "discovery_server_test.${TEST}.SHM_ON")
    list(APPEND TEST_CASE_LIST ${TEST_NAME})
    # Test with shared memory
    add_test(NAME ${TEST_NAME}
        COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
        -e $<TARGET_FILE:${PROJECT_NAME}>
        -p ${TESTS_PARAMS}
        -f ${FASTDDS_PYTHON_EXECUTABLE}
        -t ${TEST}
        -s true
        -i false) # Remove this argument to execute test with and without intraprocess
        # --force-remove # Add this argument to erase every file generated during test execution
    set_tests_properties(${TEST_NAME} PROPERTIES
        REQUIRED_FILES ${RUN_TEST}
        REQUIRED_FILES ${TESTS_PARAMS})

    if("${TEST}" IN_LIST FAIL_TEST_CASES)
        set_property(TEST ${TEST_NAME} PROPERTY LABELS xfail)
    endif()

    unset(TEST_NAME)
    set(TEST_NAME "discovery_server_test.${TEST}.SHM_OFF")
    list(APPEND TEST_CASE_LIST ${TEST_NAME})
    # Test without shared memory
    add_test(NAME ${TEST_NAME}
        COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
        -e $<TARGET_FILE:${PROJECT_NAME}>
        -p ${TESTS_PARAMS}
        -f ${FASTDDS_PYTHON_EXECUTABLE}
        -t ${TEST}
        -s false
        -i false) # Remove this argument to execute test with and without intraprocess
        # --force-remove # Add this argument to erase every file generated during test execution
    set_tests_properties(${TEST_NAME} PROPERTIES
        REQUIRED_FILES ${RUN_TEST}
        REQUIRED_FILES ${TESTS_PARAMS})

    if("${TEST}" IN_LIST FAIL_TEST_CASES)
        set_property(TEST ${TEST_NAME} PROPERTY LABELS xfail)
    endif()

endforeach()

# Windows requires an special treatment of environmental variables
if(WIN32)

    # set the environment variables to detect fast binaries
    string(REGEX MATCHALL  "[A-Z]:[^;:]+;" PATH_VARIABLE "$ENV{PATH}" )
    foreach (itvar ${PATH_VARIABLE})
            string(REPLACE ";" "" itvar ${itvar})
            set(WINDOWS_PATH_VARIABLE "${WINDOWS_PATH_VARIABLE}\\;${itvar}" )
    endforeach( itvar )

    # Generate a dll path property for each target-configuration
    get_target_property(DEPENDENCIES ${PROJECT_NAME} INTERFACE_LINK_LIBRARIES)
    unset(TEST_ENVIRONMENT_DEPENDENCIES)

    foreach( _target ${DEPENDENCIES})
        #message(STATUS "DEPENDENT TARGET ${_target}")

        if(TARGET ${_target} )

        #message(STATUS "Configurations available in ${_target} are ${CMAKE_CONFIGURATION_TYPES}\n")

        foreach( _config ${CMAKE_CONFIGURATION_TYPES} )

            string(TOUPPER  ${_config} _config) # CMake is fiercely case-sensitive
            #message(STATUS "Current config in ${_target} is ${_config}\n")

            #retrieve binary path
            get_target_property(IMPORTED_LOCATION ${_target} IMPORTED_LOCATION_${_config} )

            #message(STATUS "IMPORTED_LOCATION_${_config} = ${IMPORTED_LOCATION}")

            if(IMPORTED_LOCATION) #ignore if this configuration doesn't exist

                get_filename_component(DIR_PATH "${IMPORTED_LOCATION}" DIRECTORY )
                file(TO_NATIVE_PATH ${DIR_PATH} DIR_NATIVE_PATH)
                set_target_properties(${_target} PROPERTIES NATIVE_LOCATION_DIR_${_config} ${DIR_NATIVE_PATH} )

                #message(STATUS "Created target: ${_target} property: NATIVE_LOCATION_DIR_${_config} = ${DIR_NATIVE_PATH}")

            endif(IMPORTED_LOCATION)

        endforeach()

        if(TEST_ENVIRONMENT_DEPENDENCIES)
            set(TEST_ENVIRONMENT_DEPENDENCIES "${TEST_ENVIRONMENT_DEPENDENCIES}\\;$<TARGET_PROPERTY:${_target},NATIVE_LOCATION_DIR_$<UPPER_CASE:$<CONFIG>>>")
        else()
            set(TEST_ENVIRONMENT_DEPENDENCIES "$<TARGET_PROPERTY:${_target},NATIVE_LOCATION_DIR_$<UPPER_CASE:$<CONFIG>>>")
        endif()

        endif()
    endforeach()

    #message(STATUS ${TEST_ENVIRONMENT_DEPENDENCIES})

    # make sure the target dependencies we found override others already installed when loading dlls
    set_property(TEST ${TEST_CASE_LIST} APPEND PROPERTY ENVIRONMENT "PATH=${TEST_ENVIRONMENT_DEPENDENCIES}\\;${WINDOWS_PATH_VARIABLE}" )
endif()
