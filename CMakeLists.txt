cmake_minimum_required(VERSION 3.10.0)

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

# colcon will take care of solving any dependency issues
#find_package(Asio REQUIRED )
find_package(fastcdr   REQUIRED  )
#find_package(TinyXML2 QUIET ) # CMake is able to find it unaided into C:\Program Files\tinyxml2\lib\cmake\tinyxml2
find_package(fastrtps   REQUIRED)
find_package(OpenSSL REQUIRED)

# at present we are going to ignore all packaging, examples and test paraphernalia.

###############################################################################
# Logging
###############################################################################

IF (NOT DEFINED LOG_LEVEL_INFO)
    SET(LOG_LEVEL_INFO 0)
ENDIF()
ADD_DEFINITIONS(-DLOG_LEVEL_INFO=${LOG_LEVEL_INFO})

IF (NOT DEFINED LOG_LEVEL_WARN)
    SET(LOG_LEVEL_WARN 0)
ENDIF()
ADD_DEFINITIONS(-DLOG_LEVEL_WARN=${LOG_LEVEL_WARN})

IF (NOT DEFINED LOG_LEVEL_ERROR)
    SET(LOG_LEVEL_ERROR 1)
ENDIF()
ADD_DEFINITIONS(-DLOG_LEVEL_ERROR=${LOG_LEVEL_ERROR})

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
		)  
endif(WIN32)

# xml test files
file(GLOB ${PROJECT_NAME}_xml_tests
	resources/xml/*.xml)

# xml schema file
set(${PROJECT_NAME}_schema_files
	resources/xsd/discovery-server.xsd
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
    )

set(${PROJECT_NAME}_source_files
		#application sources
        src/main.cpp
		src/DSManager.cpp
		#library sources
		src/DI.cpp
		src/LJ.cpp
    )

# Executable and library
add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_source_files} ${${PROJECT_NAME}_header_files}
				${${PROJECT_NAME}_schema_files} ${${PROJECT_NAME}_xml_tests} ${${PROJECT_NAME}_static_types})

# schema, types and auxiliary xmls
source_group(resources\\xsd FILES ${${PROJECT_NAME}_schema_files} )
source_group(resources\\xml FILES ${${PROJECT_NAME}_xml_tests} )
source_group(resources\\static_types FILES ${${PROJECT_NAME}_static_types} )

# path to xml test TEST_XML_FILES_DIR
list(GET ${PROJECT_NAME}_xml_tests 0 TEST_XML_FILES_DIR)
get_filename_component(TEST_XML_FILES_DIR "${TEST_XML_FILES_DIR}" DIRECTORY )
file(TO_NATIVE_PATH "${TEST_XML_FILES_DIR}/" TEST_XML_FILES_DIR)


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
    PRIVATE $<BUILD_INTERFACE:${TINYXML2_INCLUDE_DIR}>
	${ASIO_INCLUDE_DIR}
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
target_link_libraries(${PROJECT_NAME} PUBLIC fastrtps fastcdr tinyxml2)

# Properties that change bin names depending on current config.
# This is convenient because on installation all bins share folder
set_target_properties(${PROJECT_NAME} PROPERTIES RELEASE_POSTFIX -${PROJECT_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX -${PROJECT_VERSION}d)

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
    EXPORT ${PROJECT_NAME}_Targets
    RUNTIME DESTINATION ${BIN_INSTALL_DIR}
    LIBRARY DESTINATION ${LIB_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
    COMPONENT bin
    )

# Install includes
install(DIRECTORY include/
    COMPONENT headers
    DESTINATION ${INCLUDE_INSTALL_DIR}
    FILES_MATCHING PATTERN "*.h"
    )

# export library
install(EXPORT ${PROJECT_NAME}_Targets
     DESTINATION ${EXPORT_INSTALL_DIR}
     )

# Sources
install(DIRECTORY ${PROJECT_SOURCE_DIR}/src/
    COMPONENT sources
    DESTINATION ${INCLUDE_SOURCE_DIR}
	FILES_MATCHING PATTERN "*.cpp"
	)

# Install licenses
install(FILES ${PROJECT_SOURCE_DIR}/license/LICENSE
    COMPONENT licenses
    DESTINATION ${LICENSE_INSTALL_DIR}
    )
	

###############################################################################
# Testing 
###############################################################################

enable_testing()

# testing enviromental variables
# add_test(NAME envtest COMMAND powershell "[environment]::GetEnvironmentVariable('PATH') > output.txt")

unset(TEST_LIST)

# add the several test, we begin for the basic discovery UDP scenario
list(APPEND TEST_LIST test_1_PDP_UDP)
add_test(NAME test_1_PDP_UDP COMMAND  ${PROJECT_NAME} ${TEST_XML_FILES_DIR}test_1_PDP_UDP.xml)
set_tests_properties(test_1_PDP_UDP PROPERTIES REQUIRED_FILES ${TEST_XML_FILES_DIR}test_1_PDP_UDP.xml)

#! TCP transport wasn't operational in my original fastRTPS commit. Enable the test after rebasing ontop
## basic discovery TCP scenario
list(APPEND TEST_LIST test_2_PDP_TCP)
add_test(NAME test_2_PDP_TCP COMMAND  ${PROJECT_NAME} ${TEST_XML_FILES_DIR}test_2_PDP_TCP.xml)
set_tests_properties(test_2_PDP_TCP PROPERTIES REQUIRED_FILES ${TEST_XML_FILES_DIR}test_2_PDP_TCP.xml DISABLED true)

# Testing late joiner detection and removal
list(APPEND TEST_LIST test_3_PDP_UDP)
add_test(NAME test_3_PDP_UDP COMMAND  ${PROJECT_NAME} ${TEST_XML_FILES_DIR}test_3_PDP_UDP.xml)
set_tests_properties(test_3_PDP_UDP PROPERTIES REQUIRED_FILES ${TEST_XML_FILES_DIR}test_3_PDP_UDP.xml)

# Testing server inter communication 
list(APPEND TEST_LIST test_4_PDP_UDP)
add_test(NAME test_4_PDP_UDP COMMAND  ${PROJECT_NAME} ${TEST_XML_FILES_DIR}test_4_PDP_UDP.xml)
set_tests_properties(test_4_PDP_UDP PROPERTIES REQUIRED_FILES ${TEST_XML_FILES_DIR}test_4_PDP_UDP.xml)

# Testing EDP discovery operation 
list(APPEND TEST_LIST test_5_EDP_UDP)
add_test(NAME test_5_EDP_UDP COMMAND  ${PROJECT_NAME} ${TEST_XML_FILES_DIR}test_5_EDP_UDP.xml)
set_tests_properties(test_5_EDP_UDP PROPERTIES REQUIRED_FILES ${TEST_XML_FILES_DIR}test_5_EDP_UDP.xml)

# Testing EDP discovery operation with late joiner detection and removal
list(APPEND TEST_LIST test_6_EDP_UDP)
add_test(NAME test_6_EDP_UDP COMMAND  ${PROJECT_NAME} ${TEST_XML_FILES_DIR}test_6_EDP_UDP.xml)
set_tests_properties(test_6_EDP_UDP PROPERTIES REQUIRED_FILES ${TEST_XML_FILES_DIR}test_6_EDP_UDP.xml)

# add_test(NAME PubSubTest COMMAND  ${PROJECT_NAME} ${TEST_XML_FILES_DIR}pub_subs_creation.xml )

# REQUIRED_FILES # associate an xml file for each test
# RUN_SERIAL

# increase the timeout to five minutes
set_tests_properties(${TEST_LIST} PROPERTIES TIMEOUT 300)

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
			
				#message(STATUS "Current config in ${_target} is ${_config}\n")
				
				#retrieve binary path
				get_target_property(IMPORTED_LOCATION ${_target} IMPORTED_LOCATION_${_config} )
				
				if(IMPORTED_LOCATION) #ignore if this configuration doesn't exist

					get_filename_component(DIR_PATH "${IMPORTED_LOCATION}" DIRECTORY )
					file(TO_NATIVE_PATH ${DIR_PATH} DIR_NATIVE_PATH)
					set_target_properties(${_target} PROPERTIES NATIVE_LOCATION_DIR_${_config} ${DIR_NATIVE_PATH} )
					
					message(STATUS "Created target: ${_target} property: NATIVE_LOCATION_DIR_${_config} = ${DIR_NATIVE_PATH}")
					
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

	#set_tests_properties(PubSubTest envtest PROPERTIES ENVIRONMENT "PATH=${WINDOWS_PATH_VARIABLE}\\;$<TARGET_FILE_DIR:fastrtps>\\;$<TARGET_FILE_DIR:fastcdr>" )
	set_tests_properties(${TEST_LIST} PROPERTIES ENVIRONMENT "PATH=${WINDOWS_PATH_VARIABLE}\\;${TEST_ENVIRONMENT_DEPENDENCIES}" )

endif()

###############################################################################
# Examples
###############################################################################
option(COMPILE_EXAMPLES "Build example" OFF)

if(COMPILE_EXAMPLES)
    add_subdirectory(examples/HelloWorldExampleDS)
endif()