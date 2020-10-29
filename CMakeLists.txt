###############################################################################
# Project and Version information											  #
###############################################################################

cmake_minimum_required(VERSION 3.10.0)

# Set CMAKE_BUILD_TYPE to Release by default if generator is single config.
if(NOT GENERATOR_IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# PROJECT_SOURCE_DIR is the source directory of the most recent project() command.
# If it exists already then this file has been added and IS_TOP_LEVEL should be FALSE.

set(IS_TOP_LEVEL TRUE)
if(PROJECT_SOURCE_DIR)
    set(IS_TOP_LEVEL FALSE)
    string(REPLACE "\\" "/" PROJECT_SOURCE_DIR ${PROJECT_SOURCE_DIR} )
endif()

# Let's see what's the latest version of the current branch
execute_process(COMMAND git describe --tags WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
      OUTPUT_VARIABLE GIT_TAG_VERSION ERROR_QUIET )

if (GIT_TAG_VERSION)
    string(REGEX MATCH "[0-9]+(\\.[0-9]+)*" PROJECT_VERSION ${GIT_TAG_VERSION} )
else()
    set(PROJECT_VERSION "0.0.1")
endif()

if (NOT PROJECT_VERSION)
    message(FATAL_ERROR
        "Project must have a version number. If not found in git, please, define PROJECT_VERSION variable.")
endif()

project("discovery-server" VERSION ${PROJECT_VERSION} LANGUAGES C CXX)

set(PROJECT_NAME_STYLED "DiscoveryServer")
set(PROJECT_NAME_LARGE "Discovery Server")
set(PROJECT_NAME_UPPER "DS")

include(CheckCXXCompilerFlag)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANG OR
        CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    check_cxx_compiler_flag(--std=c++11 SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -Wextra -pedantic")
    if(SUPPORTS_CXX11)
        add_compile_options(--std=c++11)
    else()
        message(FATAL_ERROR "Compiler doesn't support C++11")
    endif()
endif()


###############################################################################
# Test system configuration
###############################################################################
include(${PROJECT_SOURCE_DIR}/cmake/common/check_configuration.cmake)
check_stdcxx()
check_compile_feature()
check_endianness()

###############################################################################
# Check MSVC architecture
###############################################################################
if(MSVC OR MSVC_IDE)
    check_msvc_arch()
endif()

###############################################################################
# Load external dependencies
###############################################################################

# tinyxml2
# first we search for the new config files
find_package(tinyxml2 CONFIG)

# according to docs and sources the above call searching for tinyxml2 would focus on:
#   tinyxml2Config.cmake
#   tinyxml2-config.cmake
# but this is only true for unix like OS. CMake windows APIs rely on non-case-sensitive
# APIs like GetFileAttributes thus the above call will on windows find:
#   TinyXML2Config.cmake
# as a match for tinyxml2Config.cmake. That forces us to use the tinyxml2 target as a
# decision variable (old TinyXML2Config.cmake doesn't create any target).

# There are two versions of the tinyxml2Config.cmake:
# - old one: generated with the cmake export() command that defines a tinyxml2 target
#   with all config info.
# - new one: generated with the cmake install(TARGET ... EXPORT ...) and install(EXPORT ..)
#   cmake commands. This one deprecates the above one in per-target installation. Tinyxml2
#   CMakelists uses the install(EXPORT) namespace option, thus, the target is actually call
#   tinyxml2::tinyxml2.
# The tinyxml2 CMakelists.txt uses export() together with install(...) generating two independent
# tinyxml2Config.cmake files. Fortunately only installs one. We must be able to handle the existence
# of tinyxml2 and tinyxml2::tinyxml2 targets because we don't know which one is gonna find.

if(TARGET tinyxml2::tinyxml2)

    # retrieve required properties
    set(TINYXML2_LIBRARY tinyxml2::tinyxml2)
    get_target_property(TINYXML2_INCLUDE_DIR tinyxml2::tinyxml2 INTERFACE_INCLUDE_DIRECTORIES)

else()

    if(TARGET tinyxml2)

        # retrieve required properties
        set(TINYXML2_LIBRARY tinyxml2)
        get_target_property(TINYXML2_INCLUDE_DIR tinyxml2 INTERFACE_INCLUDE_DIRECTORIES)

    else()

        # redundant on Windows because matches find_package(tinyxml2 CONFIG) behaviour
        find_package(TinyXML2 CONFIG)

        if(NOT TinyXML2_FOUND)
            # we try find the binaries directly

            include(FindPackageHandleStandardArgs)

            find_path(TINYXML2_INCLUDE_DIR NAMES tinyxml2.h)
            find_library(TINYXML2_LIBRARY tinyxml2)

            find_package_handle_standard_args(tinyxml2 DEFAULT_MSG TINYXML2_LIBRARY TINYXML2_INCLUDE_DIR)

            if(NOT tinyxml2_FOUND)
                message(FATAL_ERROR "Unable to load tinyxml2 dependency. ${TINYXML2_INCLUDE_DIR}")
            endif()

        endif()

    endif()

endif()


find_package(fastcdr REQUIRED )
# new DDS API is required
find_package(fastrtps 2 REQUIRED)
find_package(OpenSSL QUIET) # according to cmake docs on FindOpenSS is hinted define OPENSSL_ROOT_DIR

#some tests require python
find_package(PythonInterp 3 REQUIRED)

#install lxml module required for some tests
execute_process(COMMAND ${Python3_EXECUTABLE} "-m pip install lxml")

# OpenSSL isn't mandatory
if(TARGET OpenSSL::SSL)
    set(OPENSSL_TARGET OpenSSL::SSL)
else()
    unset(OPENSSL_TARGET)
    unset(OPENSSL_INCLUDE_DIR CACHE)
endif()

###############################################################################
# Logging
###############################################################################

if(NOT DEFINED LOG_LEVEL_INFO)
    set(LOG_LEVEL_INFO OFF)
endif()

if(NOT DEFINED LOG_LEVEL_WARN)
    set(LOG_LEVEL_WARN OFF)
endif()

if(NOT DEFINED LOG_LEVEL_ERROR)
    SET(LOG_LEVEL_ERROR ON)
endif()

add_definitions(-DLOG_LEVEL_INFO=$<STREQUAL:$<UPPER_CASE:${LOG_LEVEL_INFO}>,ON>)
add_definitions(-DLOG_LEVEL_WARN=$<STREQUAL:$<UPPER_CASE:${LOG_LEVEL_WARN}>,ON>)
add_definitions(-DLOG_LEVEL_ERROR=$<STREQUAL:$<UPPER_CASE:${LOG_LEVEL_ERROR}>,ON>)

configure_file(${PROJECT_SOURCE_DIR}/src/DSLog.h.in ${PROJECT_SOURCE_DIR}/include/log/DSLog.h @ONLY)

###############################################################################
# Versioning
###############################################################################

# Generate a config.h with version global variables from a template and copy to the current build dir.
# project() doesn't create 0 valued version variables. We must workaround it.

foreach( version_var PROJECT_VERSION_MAJOR PROJECT_VERSION_MINOR PROJECT_VERSION_PATCH PROJECT_VERSION_TWEAK)
    if( NOT ${version_var} )
        set( ${version_var} "0 ")
        list(APPEND version_unset ${version_var})
    endif()
endforeach()

configure_file(${PROJECT_SOURCE_DIR}/src/config.h.in ${PROJECT_SOURCE_DIR}/include/version/config.h @ONLY)

# we leave the project() generated variables as they were before
foreach( version_var IN LISTS version_unset)
    unset( ${version_var} )
endforeach()
unset(version_unset)

###############################################################################
# Compilation
###############################################################################

# Create the target and its dependencies, note that variable PROJECT_NAME is created by the closest project()
# called in the current directory scope or above.
# We must populate this variables as project sources grow

# Set fast-RTPS source files

# add fast headers to enable class view on visual studio IDE
if (WIN32)
    get_target_property(FAST_INCLUDE_DIR fastrtps INTERFACE_INCLUDE_DIRECTORIES)
    list(GET FAST_INCLUDE_DIR 0 FAST_INCLUDE_DIR)
    #get_filename_component(FAST_INCLUDE_DIR "${FAST_INCLUDE_DIR}" DIRECTORY )

    file(GLOB_RECURSE ALL_FAST_HEADERS
        "${FAST_INCLUDE_DIR}/**/*.h"
        "${FAST_INCLUDE_DIR}/**/*.hpp"
        "${FAST_INCLUDE_DIR}/**/*.hxx"
        )
endif(WIN32)

# Test path
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test CACHE PATH "Source directory for test")

# xml schema file
set(${PROJECT_NAME}_schema_files
    resources/xsd/discovery-server.xsd
    resources/xsd/ds-snapshot.xsd
    )

# testing static types
file(GLOB ${PROJECT_NAME}_static_types
    resources/static_types/*.*
    )

#filter out the idl within the static types
set( ${PROJECT_NAME}_idl ${${PROJECT_NAME}_static_types} )
list(FILTER ${PROJECT_NAME}_idl INCLUDE REGEX "\\.idl$" )

set(${PROJECT_NAME}_header_files
        #TODO: remove when development process is finished
        ${ALL_FAST_HEADERS}
        # application sources
        include/version/config.h
        include/log/DSLog.h
        include/DSManager.h
        # library sources
        include/DI.h
        include/LJ.h
        include/IDs.h
    )

set(${PROJECT_NAME}_source_files
        #application sources
        src/main.cpp
        src/DSManager.cpp
        #library sources
        src/DI.cpp
        src/LJ.cpp
    )

# Executable
add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_source_files} ${${PROJECT_NAME}_header_files}
                ${${PROJECT_NAME}_schema_files} ${${PROJECT_NAME}_xml_tests}
                ${${PROJECT_NAME}_python_tests} ${${PROJECT_NAME}_static_types})

# schema, types and auxiliary xmls
source_group(resources\\xsd FILES ${${PROJECT_NAME}_schema_files} )
source_group(resources\\static_types FILES ${${PROJECT_NAME}_static_types} )

# path to xml test TEST_FILES_DIR
list(GET ${PROJECT_NAME}_tests 0 TEST_FILES_DIR)
get_filename_component(TEST_FILES_DIR "${TEST_FILES_DIR}" DIRECTORY )
file(TO_NATIVE_PATH "${TEST_FILES_DIR}/" TEST_FILES_DIR)

# path to test solution snapshots SNAPSHOT_FILES_DIR
list(GET ${PROJECT_NAME}_tests 0 SNAPSHOT_FILES_DIR)
get_filename_component(SNAPSHOT_FILES_DIR "${SNAPSHOT_FILES_DIR}" DIRECTORY )
file(TO_NATIVE_PATH "${SNAPSHOT_FILES_DIR}/" SNAPSHOT_FILES_DIR)

# path to python validation files VALIDATION_FILES_DIR
list(GET ${PROJECT_NAME}_tests 0 VALIDATION_FILES_DIR)
get_filename_component(VALIDATION_FILES_DIR "${VALIDATION_FILES_DIR}" DIRECTORY )
file(TO_NATIVE_PATH "${VALIDATION_FILES_DIR}/" VALIDATION_FILES_DIR)

# avoid visual studio from trying to compile .idl files with midl
set_source_files_properties(${${PROJECT_NAME}_idl} PROPERTIES HEADER_FILE_ONLY TRUE)

# BUILD_INTERFACE and INSTALL_INTERFACE are not variables but generator expressions
# (see under type Output-Related Expressions):
#	$<BUILD_INTERFACE:...>. Content of ... when the property is exported using export(),
# or when the target is used by another target in the same buildsystem. Expands to the empty string otherwise.
#	$<INSTALL_INTERFACE:...>. Content of ... when the property is exported using install(EXPORT), and empty otherwise.
# Relative paths are allowed within the INSTALL_INTERFACE expression and are interpreted relative to the installation
# prefix.

target_include_directories(${PROJECT_NAME} PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${TINYXML2_INCLUDE_DIR}
    ${ASIO_INCLUDE_DIR}
	${OPENSSL_INCLUDE_DIR}
    )

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ASIO_STANDALONE
    $<$<AND:$<BOOL:${WIN32}>,$<STREQUAL:"${CMAKE_SYSTEM_NAME}","WindowsStore">>:_WIN32_WINNT=0x0603>
    $<$<AND:$<BOOL:${WIN32}>,$<NOT:$<STREQUAL:"${CMAKE_SYSTEM_NAME}","WindowsStore">>>:_WIN32_WINNT=0x0601>
    )

#if (WIN32)
#    target_compile_options(${PROJECT_NAME} PRIVATE	-wd4251)
#endif()

# we link dynamically to tinyxml2
target_link_libraries(${PROJECT_NAME} PUBLIC fastrtps fastcdr
	${TINYXML2_LIBRARY}
	${OPENSSL_TARGET}
	)

# Properties that change bin names depending on current config.
# This is convenient because on installation all bins share folder
set_target_properties(${PROJECT_NAME} PROPERTIES RELEASE_POSTFIX -${PROJECT_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX -${PROJECT_VERSION}d)
set_target_properties(${PROJECT_NAME} PROPERTIES MINSIZEREL_POSTFIX -${PROJECT_VERSION}m)
set_target_properties(${PROJECT_NAME} PROPERTIES RELWITHDEBINFO_POSTFIX -${PROJECT_VERSION}rd)

###############################################################################
# Testing
###############################################################################

enable_testing()
include(CTest)  # shut up colcon's whining about .tcl files

file(COPY ${TEST_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

set(BINARY_TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/test)
file(TO_NATIVE_PATH "${BINARY_TEST_DIR}/" BINARY_TEST_DIR)

set(BINARY_TEST_CASES_DIR ${BINARY_TEST_DIR}/test_cases)
file(TO_NATIVE_PATH "${BINARY_TEST_CASES_DIR}/" BINARY_TEST_CASES_DIR)

set(BINARY_SNAPSHOTS_DIR ${BINARY_TEST_DIR}/test_solutions)
file(TO_NATIVE_PATH "${BINARY_SNAPSHOTS_DIR}/" BINARY_SNAPSHOTS_DIR)

set(RUN_TEST ${BINARY_TEST_DIR}run_test.py)

# To save time is adviceable to run ctest -j 10 to parallelized the test execution. Note that test doesn't interfere
# with each other because independent random ports are generated for the servers.

# transform the xml.in files into actual xml with random port numbers
# xml files are created into the current binary dir but installed into the test dir

# files belonging to the same test must be consecutive alphabetically
list(SORT ${PROJECT_NAME}_tests)

unset(TEST_LIST)

# Test 01
list(APPEND TEST_LIST test_01_trivial)
add_test(NAME test_01_trivial
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_01_trivial
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_01_trivial PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_01_trivial.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_01_trivial.snapshot )

# Test 02
list(APPEND TEST_LIST test_02_single_server_medium)
add_test(NAME test_02_single_server_medium
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_02_single_server_medium
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_02_single_server_medium PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_02_single_server_medium.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_02_single_server_medium.snapshot )

# Test 03
list(APPEND TEST_LIST test_03_single_server_large)
add_test(NAME test_03_single_server_large
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_03_single_server_large
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_03_single_server_large PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_03_single_server_large.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_03_single_server_large.snapshot )

# Test 04
list(APPEND TEST_LIST test_04_server_ping)
add_test(NAME test_04_server_ping
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_04_server_ping
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_04_server_ping PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_04_server_ping.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_04_server_ping.snapshot )

# Test 05
list(APPEND TEST_LIST test_05_server_double_ping)
add_test(NAME test_05_server_double_ping
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_05_server_double_ping
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_05_server_double_ping PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_05_server_double_ping.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_05_server_double_ping.snapshot )

# Test 06
list(APPEND TEST_LIST test_06_diamond_servers)
add_test(NAME test_06_diamond_servers
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_06_diamond_servers
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_06_diamond_servers PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_06_diamond_servers.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_06_diamond_servers.snapshot )

# Test 07
list(APPEND TEST_LIST test_07_server_endpoints_two_servers)
add_test(NAME test_07_server_endpoints_two_servers
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_07_server_endpoints_two_servers
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_07_server_endpoints_two_servers PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_07_server_endpoints_two_servers.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_07_server_endpoints_two_servers.snapshot )

# Test 08
list(APPEND TEST_LIST test_08_server_endpoints_four_clients)
add_test(NAME test_08_server_endpoints_four_clients
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_08_server_endpoints_four_clients
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_08_server_endpoints_four_clients PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_08_server_endpoints_four_clients.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_08_server_endpoints_four_clients.snapshot )

# Test 09
list(APPEND TEST_LIST test_09_servers_serial)
add_test(NAME test_09_servers_serial
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_09_servers_serial
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_09_servers_serial PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_09_servers_serial.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_09_servers_serial.snapshot )

# Test 10
list(APPEND TEST_LIST test_10_server_redundancy)
add_test(NAME test_10_server_redundancy
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_10_server_redundancy
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_10_server_redundancy PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_10_server_redundancy.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_10_server_redundancy.snapshot )

# Test 11
list(APPEND TEST_LIST test_11_remote_servers)
add_test(NAME test_11_remote_servers
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_11_remote_servers
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_11_remote_servers PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_11_remote_servers.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_11_remote_servers.snapshot )

# Test 12
list(APPEND TEST_LIST test_12_virtual_topics)
add_test(NAME test_12_virtual_topics
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_12_virtual_topics
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_12_virtual_topics PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_12_virtual_topics.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_12_virtual_topics.snapshot )

# Test 13
list(APPEND TEST_LIST test_13_disposals_single_server)
add_test(NAME test_13_disposals_single_server
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_13_disposals_single_server
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_13_disposals_single_server PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_13_disposals_single_server.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_13_disposals_single_server.snapshot )

# Test 14
list(APPEND TEST_LIST test_14_disposals_remote_servers)
add_test(NAME test_14_disposals_remote_servers
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_14_disposals_remote_servers
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_14_disposals_remote_servers PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_14_disposals_remote_servers.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_14_disposals_remote_servers.snapshot )

# Test 15
list(APPEND TEST_LIST test_15_disposals_client_servers)
add_test(NAME test_15_disposals_client_servers
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_15_disposals_client_servers
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_15_disposals_client_servers PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_15_disposals_client_servers.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_15_disposals_client_servers.snapshot )

# Test 16
list(APPEND TEST_LIST test_16_lease_duration_single_client)
add_test(NAME test_16_lease_duration_single_client
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_16_lease_duration_single_client
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_16_lease_duration_single_client PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_16_lease_duration_single_client.xml
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_16_lease_duration_single_client_1.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_16_lease_duration_single_client.snapshot )

# Test 17
list(APPEND TEST_LIST test_17_lease_duration_remove_client_server)
add_test(NAME test_17_lease_duration_remove_client_server
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_17_lease_duration_remove_client_server
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_17_lease_duration_remove_client_server PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_17_lease_duration_remove_client_server.xml
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_17_lease_duration_remove_client_server_1.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_17_lease_duration_remove_client_server.snapshot )

# Test 18
list(APPEND TEST_LIST test_18_disposals_remote_servers_multiprocess)
add_test(NAME test_18_disposals_remote_servers_multiprocess
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_18_disposals_remote_servers_multiprocess
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_18_disposals_remote_servers_multiprocess PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_18_disposals_remote_servers_multiprocess.xml
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_18_disposals_remote_servers_multiprocess_1.xml
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_18_disposals_remote_servers_multiprocess_2.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_18_disposals_remote_servers_multiprocess.snapshot )

# Test 19
list(APPEND TEST_LIST test_19_disposals_break_builtin_connections)
add_test(NAME test_19_disposals_break_builtin_connections
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_19_disposals_break_builtin_connections
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_19_disposals_break_builtin_connections PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_19_disposals_break_builtin_connections.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_19_disposals_break_builtin_connections.snapshot )

# Test 20
list(APPEND TEST_LIST test_20_break_builtin_connections)
add_test(NAME test_20_break_builtin_connections
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_20_break_builtin_connections
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_20_break_builtin_connections PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_20_break_builtin_connections.xml
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_20_break_builtin_connections_1.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_20_break_builtin_connections.snapshot )

# Test 21
list(APPEND TEST_LIST test_21_disposals_remote_server_trivial)
add_test(NAME test_21_disposals_remote_server_trivial
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_21_disposals_remote_server_trivial
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_21_disposals_remote_server_trivial PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_21_disposals_remote_server_trivial.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_21_disposals_remote_server_trivial.snapshot )

# Test 22: Testing setup using the ROS_DISCOVERY_SERVER environment variable
list(APPEND TEST_LIST test_22_environment_variable_setup)
add_test(NAME test_22_environment_variable_setup
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_22_environment_variable_setup
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR})
set_tests_properties(test_22_environment_variable_setup PROPERTIES
    ENVIRONMENT ROS_DISCOVERY_SERVER=127.0.0.1:22811
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_22_environment_variable_setup.xml
    REQUIRED_FILES ${BINARY_SNAPSHOTS_DIR}test_22_environment_variable_setup.snapshot )

# Test 23: Testing fast-discovery-server tool capacity of launching servers
list(APPEND TEST_LIST test_23_fast_discovery_server_tool)
add_test(NAME test_23_fast_discovery_server_tool
    COMMAND ${PYTHON_EXECUTABLE} ${RUN_TEST}
    -e $<TARGET_FILE:${PROJECT_NAME}> -t test_23_fast_discovery_server_tool
    -T ${BINARY_TEST_CASES_DIR} -g ${BINARY_SNAPSHOTS_DIR}
    -f $<$<TARGET_EXISTS:fastdds::fast-discovery-server>:$<TARGET_FILE:fastdds::fast-discovery-server>>)
set_tests_properties(test_23_fast_discovery_server_tool PROPERTIES
    REQUIRED_FILES ${RUN_TEST}
    REQUIRED_FILES ${BINARY_TEST_CASES_DIR}test_23_fast_discovery_server_tool.xml
    DISABLED $<NOT:$<TARGET_EXISTS:fastdds::fast-discovery-server>>)

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

###############################################################################
# Examples
###############################################################################
option(COMPILE_EXAMPLES "Build example" OFF)

if(COMPILE_EXAMPLES)
    add_subdirectory(examples/HelloWorldExampleDS)
endif()

###############################################################################
# Installation
###############################################################################

if(WIN32)
    set(INSTALL_PER_USER 0 CACHE BOOL "Windows per user installation mode")

    if(INSTALL_PER_USER)
        set(CMAKE_INSTALL_PREFIX "$ENV{USERPROFILE}/AppData/Local/eProsima/${PROJECT_NAME_LARGE}/" )
    endif()

endif()

set(BIN_INSTALL_DIR bin/ CACHE PATH "Installation directory for binaries")
set(INCLUDE_INSTALL_DIR include/ CACHE PATH "Installation directory for C++ headers")
set(INCLUDE_SOURCE_DIR src/ CACHE PATH "Installation directory for C++ sources")
set(EXAMPLES_INSTALL_DIR examples/ CACHE PATH "Installation directory for examples")
set(TESTS_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/test/ CACHE PATH "Installation directory for tests")
set(LIB_INSTALL_DIR lib/ CACHE PATH "Installation directory for libraries")
set(LICENSE_INSTALL_DIR license/ CACHE PATH "Installation directory for licenses")
set(EXPORT_INSTALL_DIR export/ CACHE PATH "Installation directory for cmake exports")

# install binaries
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}_Targets
    RUNTIME DESTINATION ${BIN_INSTALL_DIR}
    LIBRARY DESTINATION ${LIB_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
    COMPONENT bin
    )

# Install includes
#install(DIRECTORY include/
#    COMPONENT headers
#    DESTINATION ${INCLUDE_INSTALL_DIR}
#    FILES_MATCHING PATTERN "*.h"
#    )

# export library
install(EXPORT ${PROJECT_NAME}_Targets
     DESTINATION ${EXPORT_INSTALL_DIR}
     )

# Sources
#install(DIRECTORY ${PROJECT_SOURCE_DIR}/src/
#    COMPONENT sources
#    DESTINATION ${INCLUDE_SOURCE_DIR}
#    FILES_MATCHING PATTERN "*.cpp"
#    )


# Install licenses
install(FILES ${PROJECT_SOURCE_DIR}/license/LICENSE
    COMPONENT licenses
    DESTINATION ${LICENSE_INSTALL_DIR}
    )
