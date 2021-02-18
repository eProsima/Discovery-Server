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
        test_23_fast_discovery_server_tool
        test_24_backup
        test_25_backup_compatibility
        test_26_backup_restore
        test_27_slow_arise
        test_28_slow_arise_interconnection
        test_29_server_ping_late_joiner
        test_30_connect_locally_with_remote_entity
        test_31_matched_servers_not_share_info
        test_34_connect_locally_with_remote_server
        # test_27_tcp # Add to test TCP
    )

foreach(TEST IN LISTS TEST_LIST)

    unset(TEST_NAME)
    set(TEST_NAME "discovery_server_test.${TEST}.SHM_ON")
    # Test with shared memory
    add_test(NAME ${TEST_NAME}
        COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
        -e $<TARGET_FILE:${PROJECT_NAME}>
        -p ${TESTS_PARAMS}
        -f $<$<TARGET_EXISTS:fastdds::fast-discovery-server>:$<TARGET_FILE:fastdds::fast-discovery-server>>
        -t ${TEST}
        -s true
        -i false) # Remove this argument to execute test with and without intraprocess
        # --force-remove # Add this argument to erase every file generated during test execution
    set_tests_properties(${TEST_NAME} PROPERTIES
        REQUIRED_FILES ${RUN_TEST}
        REQUIRED_FILES ${TESTS_PARAMS})

    unset(TEST_NAME)
    set(TEST_NAME "discovery_server_test.${TEST}.SHM_OFF")
    # Test without shared memory
    add_test(NAME ${TEST_NAME}
        COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
        -e $<TARGET_FILE:${PROJECT_NAME}>
        -p ${TESTS_PARAMS}
        -f $<$<TARGET_EXISTS:fastdds::fast-discovery-server>:$<TARGET_FILE:fastdds::fast-discovery-server>>
        -t ${TEST}
        -s false
        -i false) # Remove this argument to execute test with and without intraprocess
        # --force-remove # Add this argument to erase every file generated during test execution
    set_tests_properties(${TEST_NAME} PROPERTIES
        REQUIRED_FILES ${RUN_TEST}
        REQUIRED_FILES ${TESTS_PARAMS})

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
    set_property(TEST ${TEST_LIST} APPEND PROPERTY ENVIRONMENT "PATH=${TEST_ENVIRONMENT_DEPENDENCIES}\\;${WINDOWS_PATH_VARIABLE}" )

endif()
