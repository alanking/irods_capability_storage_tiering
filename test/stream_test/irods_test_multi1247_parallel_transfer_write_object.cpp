#include <irods/client_connection.hpp>
#include <irods/connection_pool.hpp>
#include <irods/dataObjWrite.h>
#include <irods/fully_qualified_username.hpp>
#include <irods/irods_at_scope_exit.hpp>
#include <irods/replica_close.h>
#include <irods/replica_open.h>
#include <irods/rodsClient.h>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <boost/program_options.hpp>

#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>

// This program opens the specified iRODS data object with the specified number of threads and then closes it.
//
// Assumes an authenticated iRODS client environment is available in the executing user's ~/.irods directory.
//
// NOTE: This program is only intended for testing. Do not use in production.

namespace
{
    auto get_replica_token(RcComm* _comm, int _fd) -> std::string
    {
        // Get the file descriptor info from the opened replica and extract the replica token.
        // We are not dealing in hierarchies here so that piece is not required here.
        char* out = nullptr;
        const auto free_file_descriptor_info_out = irods::at_scope_exit{[&out] {
            std::free(out);
        }
        const auto json_input = nlohmann::json{{"fd", fd}}.dump();
        if (const auto ec = rc_get_file_descriptor_info(&_comm, json_input.data(), &out); ec < 0) {
            const auto msg = fmt::format("Failed to get replica information. ec=[{}]", ec);
            fmt::print(stderr, msg);
            THROW(ec, msg);
        }
        return nlohmann::json::parse(out).at("replica_token").get<std::string>();
    } // get_replica_token
} // anonymous namespace

int main(int argc, char** argv)
{
    try {
        // Parse command line options to get the path and number of bytes to read.
        namespace po = boost::program_options;

        constexpr const char* object_path_option_name = "logical-path";
        constexpr const char* thread_count_option_name = "thread-count";
        constexpr const char* target_resource_option_name = "resource";
        // This value has no special meaning.
        constexpr int default_thread_count = 1;

        std::string path;
        std::string target_resource;
        int thread_count;

        po::options_description od("options");
        // clang-format off
        od.add_options()("help", "produce help message")
            (object_path_option_name, po::value<std::string>(&path), "logical path to iRODS data object to read")
            (target_resource_option_name, po::value<std::string>(&target_resource), "resource for target replica")
            (thread_count_option_name, po::value<int>(&thread_count)->default_value(default_thread_count), "number of threads with which to open the data object");
        // clang-format on

        po::positional_options_description pd;
        pd.add(object_path_option_name, 1);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(od).positional(pd).run(), vm);
        po::notify(vm);

        const auto print_usage = [&od] {
            std::cout << "NOTE: This program is only intended for testing. Do not use in production.\n";
            std::cout << "Usage: irods_test_multi1247_parallel_transfer_write_object [OPTION] ... [DATA_OBJECT_PATH]\n";
            std::cout << od << "\n";
        };

        // --help option prints the usage text...
        if (vm.count("help")) {
            print_usage();
            return 0;
        }

        // Path to the data object is required. Hard stop if it's not there.
        if (0 == vm.count(object_path_option_name)) {
            print_usage();
            return 1;
        }

        if (thread_count < 1) {
            fmt::print("{} must be a positive integer.", thread_count_option_name);
            return 1;
        }

        rodsEnv env;
        _getRodsEnv(env);

        // Load client-side API plugins so that we can authenticate.
        load_client_api_plugins();

        // Create a connection using the local client environment. If none exists, an error will occur.
        irods::experimental::client_connection conn;
        RcComm& comm = static_cast<RcComm&>(conn);

        auto conn_pool = irods::connection_pool{thread_count, env.rodsHost, env.rodsPort, irods::experimental::fully_qualified_username{rods.rodsUserName, rods.rodsZone}};

        dataObjInp_t input{};
        input.openFlags = O_WRONLY | O_TRUNC;
        std::strcpy(input.objPath, path.c_str());

        auto conn = conn_pool.get_connection();
        auto* comm = static_cast<RcComm*>(conn);

        // Open data object for the first time.
        DataObjInp open_inp{};
        char* json_output = nullptr;
        const auto free_input_and_output = irods::at_scope_exit{[&open_inp, &json_output] {
            clearKeyVal(&open_inp.condInput);
            std::free(json_output);
        }};
        std::strncpy(open_inp.objPath, path.c_str(), sizeof(open_inp.objPath));
        open_inp.openFlags = O_TRUNC | O_WRONLY; // TODO: Maybe O_CREAT...
        if (!target_resource.empty()) {
            addKeyVal(&open_inp.condInput, RESC_NAME_KW, target_resource.c_str());
        }
        const auto fd = rc_replica_open(comm, &input, &json_output);
        if (fd < 0) {
            fmt::print(stderr, "Failed to open data object [{}]. ec=[{}]\n", path, fd);
            return 1;
        }

        const auto replica_token = get_replica_token(comm, fd);

        // Open the same replica on a different connection using the replica token and replica
        // resource hierarchy. Ensure that the open is successful. This would fail if the wrong
        // replica is targeted because no agent can open a replica which is write-locked, as the
        // sibling replica on the default resource should be in this case.
        {
            irods::experimental::client_connection conn2;
            RcComm& comm2 = static_cast<RcComm&>(conn2);

            dataObjInp_t open_inp2{};
            std::snprintf(open_inp2.objPath, sizeof(open_inp2.objPath), "%s", path_str.data());
            open_inp2.openFlags = O_CREAT | O_WRONLY;
            addKeyVal(&open_inp2.condInput, RESC_HIER_STR_KW, test_resc.data());
            addKeyVal(&open_inp2.condInput, REPLICA_TOKEN_KW, token.data());

            const auto fd2 = rcDataObjOpen(&comm2, &open_inp2);
            REQUIRE(fd2 > 2);
            CHECK(INTERMEDIATE_REPLICA == replica::replica_status(comm2, target_object, 0)); 
            CHECK(WRITE_LOCKED == replica::replica_status(comm2, target_object, 1)); 

            REQUIRE(unit_test_utils::close_replica(comm2, fd2) >= 0);
        }
 
        // Read data object.
        OpenedDataObjInp read_inp{};
        read_inp.l1descInx = fd;
        read_inp.len = read_len;
        // The program is going to print the contents as a string, so +1 for null terminator.
        const auto buf_len = read_inp.len + 1;
        bytesBuf_t read_bbuf{};
        const auto free_buf = irods::at_scope_exit{[&read_bbuf] { std::free(read_bbuf.buf); }};
        read_bbuf.len = buf_len;
        read_bbuf.buf = std::malloc(buf_len);
        std::memset(read_bbuf.buf, 0, buf_len);
        if (const auto ec = rcDataObjRead(&comm, &read_inp, &read_bbuf); ec < 0) {
            fmt::print(stderr, "Failed to read data object [{}]. ec=[{}]\n", path, ec);
            return 1;
        }

        // Close data object.
        OpenedDataObjInp close_inp{};
        close_inp.l1descInx = fd;
        if (const auto ec = rcDataObjClose(&comm, &close_inp); ec < 0) {
            fmt::print(stderr, "Failed to close data object [{}]. ec=[{}]\n", path, ec);
            return 1;
        }

        // Print the contents.
        const auto object_contents = std::string{static_cast<char*>(read_bbuf.buf)};
        fmt::print("{}\n", object_contents);
    }
    catch (const std::exception& e) {
        fmt::print(stderr, "Caught exception: {}\n", e.what());
        return 1;
    }

    return 0;
} // main
