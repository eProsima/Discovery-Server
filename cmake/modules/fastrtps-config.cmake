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

set(fastrtps_VERSION 1.7.1)


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was Config.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

# Note that I cannot distinguish the Debug and Release folders because CMake (using visual studio doesn't make any distinction).
# The variables CMAKE_BUILD_TYPE, CMAKE_RUNTIME_OUTPUT_DIRECTORY are empty for visual studio run. The way CMake workarounds this situation is by generating binaries with different names in the installation dirs.
# But the build structure doesn't use common installation dirs so we cannot make <package>_BIN_DIR or <package>_LIB_DIR directly reference this folders from here.
# The better way of workaround it is see what is the eventual use given to this variables (in order to discover this I leave them unset) because the really useful variables are target linked and introduced by <package>-targets.cmake

#set_and_check(fastrtps_BIN_DIR "C:/Program Files/fastrtps/bin")
#set_and_check(fastrtps_INCLUDE_DIR "C:/Program Files/fastrtps//include")
#set_and_check(fastrtps_LIB_DIR "C:/Program Files/fastrtps//lib")

find_package(fastcdr REQUIRED)
find_package(TinyXML2 QUIET)


include(${CMAKE_CURRENT_LIST_DIR}/fastrtps-targets.cmake)
