#include "command_line_parser.hpp"
#include "dump_uploader.hpp"
#include "http_server.hpp"

#include <exec/static_thread_pool.hpp>
#include <utilities.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
using namespace chai;
constexpr const char* trustStorePath = "/etc/ssl/certs/authority";
constexpr const char* x509Comment = "Generated from OpenBMC service";
using namespace redfish;

int main(int argc, const char* argv[])
{
    // Create a server endpoint
    auto [port, root] = getArgs(parseCommandline(argc, argv), "-p", "-r");
    if (port.empty())
    {
        std::cout << "Invalid arguments\n";
        std::cout << "eg: fileserver -p port\n";
        return 0;
    }
    exec::static_thread_pool threadPool;
#ifdef SSL_ON
    HttpsServer server(
        port, "/Users/abhilashraju/work/cpp/chai/certs/server-certificate.pem",
        "/Users/abhilashraju/work/cpp/chai/certs/server-private-key.pem",
        "/etc/ssl/certs/authority");
#else
    HttpServer server(port);
#endif

    DumpUploader uploader(server.router(), root);
    server.start(threadPool);
    return 0;
}
