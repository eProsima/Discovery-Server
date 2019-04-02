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
	message(FATAL_ERROR "Project must have a version number. If not found in git, please, define PROJECT_VERSION variable.")
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

# UNTIL REACHING COLCON AWARENESS WE WILL GIVE SOME LOCAL CLUES TO CMAKE TO FIND PACKAGES ;-) ;-) ;-) ;-) ;-) ;-) ;-) 
set(CMAKE_PREFIX_PATH ${PROJECT_SOURCE_DIR}/cmake/modules ${CMAKE_PREFIX_PATH} )
# CONFIG FILES CAN BE FOUND IN DIRECTORY MODULES, REMEMBER TO REMOVE THEM WHEN COLCON ENABLED: 
#	fastcdr-config-version.cmake	fastrtps-config-version.cmake
#	fastcdr-config.cmake			fastrtps-config.cmake
#	fastcdr-targets.cmake			fastrtps-targets.cmake
#
# ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) ;-) 


# colcon will take care of solving any dependency issues
find_package(Asio REQUIRED )
find_package(fastcdr   REQUIRED  )
find_package(TinyXML2 QUIET ) # CMake is able to find it unaided into C:\Program Files\tinyxml2\lib\cmake\tinyxml2
find_package(fastrtps   REQUIRED)

# at present we are going to ignore all packaging, examples and test paraphernalia.

# we configure server log level, note the definitions which are pass to the compiler
# error logs defaults to ON

IF (NOT DEFINED LOG_LEVEL_INFO)
    SET(LOG_LEVEL_INFO OFF)
ENDIF()
ADD_DEFINITIONS(-DLOG_LEVEL_INFO=${LOG_LEVEL_INFO})

IF (NOT DEFINED LOG_LEVEL_WARN)
    SET(LOG_LEVEL_WARN OFF)
ENDIF()
ADD_DEFINITIONS(-DLOG_LEVEL_WARN=${LOG_LEVEL_WARN})

IF (NOT DEFINED LOG_LEVEL_ERROR)
    SET(LOG_LEVEL_ERROR ON)
ENDIF()
ADD_DEFINITIONS(-DLOG_LEVEL_ERROR=${LOG_LEVEL_ERROR})

# Generate the version and log header files

configure_file(${PROJECT_SOURCE_DIR}/src/DSLog.h.in ${PROJECT_SOURCE_DIR}/include/log/DSLog.h @ONLY)

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

# Create the target and its dependencies, note that variable PROJECT_NAME is created by the closest project() called in the current directory scope or above.
# We must populate this variables as project sources grow

# Set fast-RTPS source files
file(GLOB_RECURSE ALL_FAST_HEADERS 
	"C:/Users/MiguelBarro/Documents/Fast-RTPS/include/**/*.h"
	"C:/Users/MiguelBarro/Documents/Fast-RTPS/include/**/*.hpp"
	)  #TODO: remove when development process is finished

set(${PROJECT_NAME}_header_files
		#TODO: remove when development process is finished
		${ALL_FAST_HEADERS}
		# application sources
        include/version/config.h
        include/log/DSLog.h
		include/DSManager.h
		# library sources
		include/PDPServer.h
		include/DServerEvent.h
		include/EDPServer.h
		include/PDPServerListener.h
		include/EDPServerListeners.h
		include/DI.h
    )

set(${PROJECT_NAME}_source_files
		#application sources
        src/main.cpp
		src/DSManager.cpp
		#library sources
		src/PDPServer.cpp
		src/DServerEvent.cpp
		src/EDPServer.cpp
		src/PDPServerListener.cpp
		src/EDPServerListeners.cpp
		src/DI.cpp
    )

# Executable and library
add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_source_files} ${${PROJECT_NAME}_header_files})

# BUILD_INTERFACE and INSTALL_INTERFACE are not variables but generator expressions (see under type Output-Related Expressions):
#	$<BUILD_INTERFACE:...>. Content of ... when the property is exported using export(), or when the target is used by another target in the same buildsystem. Expands to the empty string otherwise.
#	$<INSTALL_INTERFACE:...>. Content of ... when the property is exported using install(EXPORT), and empty otherwise. Relative paths are allowed within the INSTALL_INTERFACE expression and are interpreted relative to the installation prefix.

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

target_compile_options(${PROJECT_NAME} PRIVATE	-wd4251) 

# we link dynamically to tinyxml2
target_link_libraries(${PROJECT_NAME} PUBLIC fastrtps fastcdr tinyxml2 )

# Properties that change bin names depending on current config. This is convenient because on installation all bins share folder
set_target_properties(${PROJECT_NAME} PROPERTIES RELEASE_POSTFIX -${PROJECT_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX -${PROJECT_VERSION}d)

# old fashion installation

if(WIN32)
	set(INSTALL_PER_USER true CACHE BOOL "Windows per user installation mode")
	
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
	
	
