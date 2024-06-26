# Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
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

cmake_minimum_required(VERSION 3.22)

project(HelloWorldExampleDS)

# Find requirements
if(NOT fastcdr_FOUND)
    find_package(fastcdr REQUIRED)
endif()

if(NOT fastdds_FOUND)
    find_package(fastdds REQUIRED)
endif()

# Set C++11
include(CheckCXXCompilerFlag)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANG OR
        CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    check_cxx_compiler_flag(-std=c++11 SUPPORTS_CXX11)
    if(SUPPORTS_CXX11)
        add_compile_options(-std=c++11)
    else()
        message(FATAL_ERROR "Compiler doesn't support C++11")
    endif()
endif()

message(STATUS "Configuring HelloWorld example...")
file(GLOB HELLOWORLD_EXAMPLE_SOURCES_CXX "*.cxx")
file(GLOB HELLOWORLD_EXAMPLE_SOURCES_CPP "*.cpp")
file(GLOB HELLOWORLD_EXAMPLE_HEADERS "*.h")

add_executable(HelloWorldExampleDS ${HELLOWORLD_EXAMPLE_SOURCES_CXX} ${HELLOWORLD_EXAMPLE_SOURCES_CPP}
	${HELLOWORLD_EXAMPLE_HEADERS} ${ALL_FAST_HEADERS})

target_link_libraries(HelloWorldExampleDS fastdds fastcdr)

# Properties that change bin names depending on current config.
# This is convenient because on installation all bins share folder
set_target_properties(HelloWorldExampleDS PROPERTIES DEBUG_POSTFIX -d)

install(TARGETS HelloWorldExampleDS
    RUNTIME DESTINATION examples/C++/HelloWorldExampleDS/${BIN_INSTALL_DIR})

