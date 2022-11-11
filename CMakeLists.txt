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
execute_process(COMMAND ${Python3_EXECUTABLE} "-m pip install lxml xmltodict jsondiff")

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

option(LOG_LEVEL_INFO OFF)
option(LOG_LEVEL_WARN OFF)
option(LOG_LEVEL_ERROR OFF)

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
# Activate Sanitizers
###############################################################################
option(SANITIZER "Enable Thread or Address sanitizers" OFF)
string(TOUPPER ${SANITIZER} SANITIZER)

if (SANITIZER)
    if(${SANITIZER} STREQUAL "ADDRESS")
        message(STATUS "Enabling address sanitizer...")
        # Warning/Error messages
        if(NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"))
            message(WARNING "Address sanitizer results with an optimized (non-Debug) build may be misleading")
        endif()

        if("${CMAKE_C_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang")
            message(STATUS "Building with llvm Address sanitizer Tools")

            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")

        elseif(CMAKE_COMPILER_IS_GNUCXX)
            message(STATUS "Building with Address sanitizer Tools")

            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")

        else()
            message(FATAL_ERROR "Address sanitizer requires Clang or GCC. Aborting.")
        endif()

    elseif(${SANITIZER} STREQUAL "THREAD")
        message(STATUS "Enabling thread sanitizer...")
        # Warning/Error messages
        if(NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"))
            message(WARNING "Thread sanitizer results with an optimized (non-Debug) build may be misleading")
        endif()

        if("${CMAKE_C_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang")
            message(STATUS "Building with llvm Thread sanitizer Tools")

            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")

        elseif(CMAKE_COMPILER_IS_GNUCXX)
            message(STATUS "Building with Thread sanitizer Tools")

            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread -fno-omit-frame-pointer")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -fno-omit-frame-pointer")

        else()
            message(FATAL_ERROR "Thread sanitizer requires Clang or GCC. Aborting.")
        endif()

    else()
        message(WARNING "Sanitizer option not supported. Continuing without any code instrumentation... ")

    endif()
endif()

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
        include/DiscoveryServerManager.h
        # library sources
        include/DiscoveryItem.h
        include/LateJoiner.h
        include/IDs.h
    )

set(${PROJECT_NAME}_source_files
        #application sources
        src/main.cpp
        src/DiscoveryServerManager.cpp
        #library sources
        src/DiscoveryItem.cpp
        src/LateJoiner.cpp
    )

# Executable
add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_source_files} ${${PROJECT_NAME}_header_files}
                ${${PROJECT_NAME}_python_tests} ${${PROJECT_NAME}_static_types})

# schema, types and auxiliary xmls
source_group(resources\\xsd FILES ${${PROJECT_NAME}_schema_files} )
source_group(resources\\static_types FILES ${${PROJECT_NAME}_static_types} )

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
include(CTest)
add_subdirectory(test)

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
set(LIB_INSTALL_DIR lib/ CACHE PATH "Installation directory for libraries")
set(LICENSE_INSTALL_DIR license/ CACHE PATH "Installation directory for licenses")
set(EXPORT_INSTALL_DIR export/ CACHE PATH "Installation directory for cmake exports")

# install binaries
install(TARGETS ${PROJECT_NAME}
    COMPONENT bin
    EXPORT ${PROJECT_NAME}_Targets
    RUNTIME DESTINATION ${BIN_INSTALL_DIR}
    LIBRARY DESTINATION ${LIB_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
    )

# Install includes
#install(DIRECTORY include/
#    COMPONENT headers
#    DESTINATION ${INCLUDE_INSTALL_DIR}
#    FILES_MATCHING PATTERN "*.h"
#    )

# export library
install(EXPORT ${PROJECT_NAME}_Targets
     COMPONENT bin
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
