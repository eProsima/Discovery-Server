cmake_minimum_required(VERSION 3.13.0)

###############################################################################
# Project and Version information											  #
###############################################################################

# PROJECT_SOURCE_DIR is the source directory of the most recent project() command.
# If it exists already then this file has been added and IS_TOP_LEVEL should be FALSE.

set(IS_TOP_LEVEL TRUE)
if(PROJECT_SOURCE_DIR)
    set(IS_TOP_LEVEL FALSE)
    string(REPLACE "\\" "/" PROJECT_SOURCE_DIR ${PROJECT_SOURCE_DIR} )
endif()

# Let's see what's the latest version of the current branch
execute_process(COMMAND git describe --tags WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_TAG_VERSION )
string(REGEX MATCH "[0-9]+(\\.[0-9]+)*" PROJECT_VERSION ${GIT_TAG_VERSION} )

if (NOT PROJECT_VERSION)
	message(FATAL_ERROR "Project must have a version number. If not using git define PROJECT_VERSION variable.")
endif()

project("discovery-server" VERSION ${PROJECT_VERSION} LANGUAGES C CXX)

# Generate a version.h with version global variables from a template and copy to the current build dir.
# project() doesn't create 0 valued version variables. We must workaround it.

foreach( version_var PROJECT_VERSION_MAJOR PROJECT_VERSION_MINOR PROJECT_VERSION_PATCH PROJECT_VERSION_TWEAK)
	if( NOT ${version_var} )
		set( ${version_var} "0 ")
	endif()
endforeach()
 
configure_file(version.h.in version.h @ONLY)

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

# UNTIL REACHING COLCON AWARENESS WE WILL GIVE SOME LOCAL CLUES TO CMAKE TO FIND PACKAGES ;-) ;-) ;-) ;-) ;-) ;-) ;-) 
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake/modules)
# IN DIRECTORY MODULES CAN BE FOUND, REMEMBER TO REMOVE THEM WHEN COLCON ENABLED: 
#	fastcdr-config-version.cmake	fastrtps-config-version.cmake
#	fastcdr-config.cmake			fastrtps-config.cmake
#	fastcdr-targets.cmake			fastrtps-targets.cmake
#
# ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) 

# colcon will take care of solving any dependency issues
find_package(fastcdr  REQUIRED)
find_package(fastrtps REQUIRED)












