cmake_minimum_required(VERSION 3.12...3.18 FATAL_ERROR)
if (POLICY CMP0109)
  # find_program requires permission to execute but not to read
  cmake_policy(SET CMP0109 NEW)
endif()
if (POLICY CMP0121)
  # Detect invalid indices in list()
  cmake_policy(SET CMP0121 NEW)
endif()
if (POLICY CMP0125)
  # Consistent behavior for cache variables managed by find_*()
  cmake_policy(SET CMP0125 NEW)
endif()
if (POLICY CMP0130)
  # Diagnose condition evaluation errors in while()
  cmake_policy(SET CMP0130 NEW)
endif()
if (POLICY CMP0132)
  # Consistent handling of compiler environment variables
  cmake_policy(SET CMP0132 NEW)
endif()
if (POLICY CMP0140)
  # Check validity of return() params
  cmake_policy(SET CMP0140 NEW)
endif()

set(IRODS_MINIMUM_VERSION "4.90.0")
find_package(IRODS "${IRODS_MINIMUM_VERSION}" REQUIRED)
set(IRODS_PLUGIN_REVISION "0")
set(IRODS_PLUGIN_VERSION "5.0.0.${IRODS_PLUGIN_REVISION}")

set(IRODS_PACKAGE_REVISION "0")

include(IrodsCXXCompiler)
set(CMAKE_CXX_STANDARD ${IRODS_CXX_STANDARD})
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-Wl,--enable-new-dtags -Wl,--as-needed")
set(CMAKE_MODULE_LINKER_FLAGS_RELEASE_INIT "-Wl,--gc-sections -Wl,-z,combreloc")
include(IrodsRunpathDefaults)

set(IRODS_TARGET_NAME_PREFIX irods_rule_engine_plugin)

project(irods_capability-storage_tiering
	VERSION "${IRODS_PLUGIN_VERSION}"
	LANGUAGES CXX)

include("${IRODS_TARGETS_PATH}")

include(GNUInstallDirs)

if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build {Debug, Release}." FORCE)
	message(STATUS "Setting unspecified CMAKE_BUILD_TYPE to '${CMAKE_BUILD_TYPE}'")
endif()

set(IRODS_BUILD_WITH_WERROR OFF CACHE BOOL "Choose whether to compile with -Werror.")

if (IRODS_BUILD_WITH_WERROR)
	add_compile_options(-Werror)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fpermissive>)
endif()

option(IRODS_ENABLE_ADDRESS_SANITIZER "Enables detection of memory leaks and other features provided by Address Sanitizer." OFF)
if (IRODS_ENABLE_ADDRESS_SANITIZER)
  # Make sure the correct llvm-symbolizer binary is available to Address Sanitizer. This binary
  # allows debug symbols to be reported appropriately. There are two ways to do this:
  #
  #     export PATH=/opt/irods-externals/clang13.0.1-0/bin:$PATH
  #
  # - or -
  #
  #     export ASAN_SYMBOLIZER_PATH=/opt/irods-externals/clang13.0.1-0/bin/llvm-symbolizer
  #
  # detect_container_overflow is disabled to guard against false positives which occur when
  # parts of the binary are compiled with ASAN and other parts are not.
  add_compile_definitions(IRODS_ADDRESS_SANITIZER_DEFAULT_OPTIONS="log_path=/tmp/irods_storage_tiering_asan_output:detect_container_overflow=0")
  add_compile_options(
    -fsanitize=address
    -fno-omit-frame-pointer
    -fno-optimize-sibling-calls
    -O1)
  add_link_options(
    -fsanitize=address
    -fno-omit-frame-pointer
    -fno-optimize-sibling-calls
    -O1)
else()
  set(CMAKE_MODULE_LINKER_FLAGS_INIT "${CMAKE_MODULE_LINKER_FLAGS_INIT} -Wl,-z,defs")
endif()

if (NOT DEFINED THREADS_PREFER_PTHREAD_FLAG)
	set(THREADS_PREFER_PTHREAD_FLAG TRUE)
endif()
find_package(Threads REQUIRED)
find_package(nlohmann_json "3.6.1" REQUIRED)
find_package(OpenSSL REQUIRED)

set(IRODS_POLICY_NAME "unified_storage_tiering")
set(IRODS_PLUGIN_TARGET_NAME "${IRODS_TARGET_NAME_PREFIX}-${IRODS_POLICY_NAME}")

string(REPLACE "_" "-" IRODS_POLICY_NAME_HYPHENS "${IRODS_POLICY_NAME}")
set(IRODS_POLICY_PACKAGE_COMPONENT "${IRODS_POLICY_NAME_HYPHENS}")

add_subdirectory(test)

add_library(
	"${IRODS_PLUGIN_TARGET_NAME}"
	MODULE
	"${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/storage_tiering.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/configuration.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/utilities.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/data_verification_utilities.cpp"
)
target_link_libraries(
	"${IRODS_PLUGIN_TARGET_NAME}"
	PRIVATE
	irods_common
	irods_server
	nlohmann_json::nlohmann_json
	"${IRODS_EXTERNALS_FULLPATH_BOOST}/lib/libboost_system.so"
	"${IRODS_EXTERNALS_FULLPATH_BOOST}/lib/libboost_regex.so"
	"${IRODS_EXTERNALS_FULLPATH_BOOST}/lib/libboost_filesystem.so"
)
target_include_directories(
	"${IRODS_PLUGIN_TARGET_NAME}"
	PRIVATE
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
	"${IRODS_EXTERNALS_FULLPATH_BOOST}/include"
)
target_compile_definitions(
	"${IRODS_PLUGIN_TARGET_NAME}"
	PRIVATE
	BOOST_SYSTEM_NO_DEPRECATED
	RODS_SERVER
	ENABLE_RE
	IRODS_ENABLE_SYSLOG
	${IRODS_COMPILE_DEFINITIONS}
	${IRODS_COMPILE_DEFINITIONS_PRIVATE}
)
target_compile_options(
	"${IRODS_PLUGIN_TARGET_NAME}"
	PRIVATE
	-Wno-write-strings
)

install(
	TARGETS
	"${IRODS_PLUGIN_TARGET_NAME}"
	LIBRARY
	DESTINATION "${IRODS_PLUGINS_DIRECTORY}/rule_engines"
	COMPONENT "${IRODS_POLICY_PACKAGE_COMPONENT}"
)

install(
	FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/packaging/test_plugin_unified_storage_tiering.py"
	DESTINATION "${IRODS_HOME_DIRECTORY}/scripts/irods/test"
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
	COMPONENT "${IRODS_POLICY_PACKAGE_COMPONENT}"
 )

install(
	FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/packaging/run_unified_storage_tiering_plugin_test.py"
	DESTINATION "${IRODS_HOME_DIRECTORY}/scripts"
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
	COMPONENT "${IRODS_POLICY_PACKAGE_COMPONENT}"
)

install(
	FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/packaging/example_unified_tiering_invocation.r"
	DESTINATION "${IRODS_HOME_DIRECTORY}"
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
	COMPONENT "${IRODS_POLICY_PACKAGE_COMPONENT}"
)

if (NOT CPACK_GENERATOR)
	set(CPACK_GENERATOR ${IRODS_CPACK_GENERATOR} CACHE STRING "CPack generator to use, e.g. {DEB, RPM, TGZ}." FORCE)
	message(STATUS "Setting unspecified CPACK_GENERATOR to ${CPACK_GENERATOR}. This is the correct setting for normal builds.")
endif()

# CPACK_DEBIAN_PACKAGE_VERSION was previously a cache variable. Evict it.
unset(CPACK_DEBIAN_PACKAGE_VERSION CACHE)

include(IrodsCPackCommon)

list(APPEND CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "/usr/sbin")
list(APPEND CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "${CPACK_PACKAGING_INSTALL_PREFIX}${IRODS_HOME_DIRECTORY}")
list(APPEND CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "${CPACK_PACKAGING_INSTALL_PREFIX}${IRODS_HOME_DIRECTORY}/scripts")
list(APPEND CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "${CPACK_PACKAGING_INSTALL_PREFIX}${IRODS_HOME_DIRECTORY}/scripts/irods")
list(APPEND CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "${CPACK_PACKAGING_INSTALL_PREFIX}${IRODS_HOME_DIRECTORY}/scripts/irods/test")

string(REPLACE "_" "-" IRODS_PLUGIN_TARGET_NAME_HYPHENS "${IRODS_PLUGIN_TARGET_NAME}")

set(CPACK_PACKAGE_NAME "${IRODS_PLUGIN_TARGET_NAME_HYPHENS}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_COMPONENTS_GROUPING IGNORE)
set(CPACK_PACKAGE_VERSION ${IRODS_PLUGIN_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "The integrated Rule-Oriented Data System")

set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "irods-server (= ${IRODS_VERSION}), irods-runtime (= ${IRODS_VERSION})")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_SOURCE_DIR}/packaging/postinst;")

set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_RPM_PACKAGE_LICENSE "BSD-3-Clause")
set(CPACK_RPM_PACKAGE_AUTOREQ 0)
set(CPACK_RPM_PACKAGE_AUTOPROV 0)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_REQUIRES "irods-server = ${IRODS_VERSION}, irods-runtime = ${IRODS_VERSION}")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/packaging/postinst")

include(CPack)
