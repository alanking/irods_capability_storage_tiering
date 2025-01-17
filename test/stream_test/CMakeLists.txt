set(_IRODS_TEST_EXECUTABLES_BUILD_DOCSTR "Build and package executables made for testing.")

# Building the test executables is enabled by default for Debug builds.
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(_IRODS_TEST_EXECUTABLES_BUILD_DEFAULT YES)
else()
	set(_IRODS_TEST_EXECUTABLES_BUILD_DEFAULT NO)
endif()

set(IRODS_TEST_EXECUTABLES_BUILD ${_IRODS_TEST_EXECUTABLES_BUILD_DEFAULT} CACHE BOOL "${_IRODS_TEST_EXECUTABLES_BUILD_DOCSTR}")

if (NOT IRODS_TEST_EXECUTABLES_BUILD)
	return()
endif()

add_executable(
	irods_test_read_object
	"${CMAKE_CURRENT_SOURCE_DIR}/irods_test_read_object.cpp"
)
target_link_libraries(
	irods_test_read_object
	PRIVATE
	irods_client
	irods_common
	irods_plugin_dependencies
	"${IRODS_EXTERNALS_FULLPATH_BOOST}/lib/libboost_program_options.so"
)
target_include_directories(
	irods_test_read_object
	PRIVATE
	"${IRODS_EXTERNALS_FULLPATH_BOOST}/include"
)
target_compile_definitions(
    irods_test_read_object
	PRIVATE
	${IRODS_COMPILE_DEFINITIONS}
	${IRODS_COMPILE_DEFINITIONS_PRIVATE}
)
install(
	TARGETS
	irods_test_read_object
	RUNTIME
	DESTINATION "${CMAKE_INSTALL_SBINDIR}"
	COMPONENT "${IRODS_POLICY_PACKAGE_COMPONENT}"
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
