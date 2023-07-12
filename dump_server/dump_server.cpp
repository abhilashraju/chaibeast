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
    auto [port,root] = getArgs(parseCommandline(argc, argv), "-p","-r");
    if (port.empty())
    {
        std::cout << "Invalid arguments\n";
        std::cout << "eg: fileserver -p port\n";
        return 0;
    }
    exec::static_thread_pool threadPool;
#ifdef SSL_ON
    HttpsServer server(
        port, "/etc/ssl/certs/https/server.pem",
+            "/etc/ssl/certs/https/server.pem", "/etc/ssl/certs/authority");
#else
    HttpServer server(port);
#endif

    auto plain_text_handler = [](auto func) {
        return [func = std::move(func)](const auto& req, const auto& httpfunc) {
            http::response<http::string_body> resp{http::status::ok,
                                                   version(req)};
            resp.set(http::field::content_type, "text/plain");
            resp.body() = func(req, httpfunc);
            resp.set(http::field::content_length,
                     std::to_string(resp.body().length()));
            return resp;
        };
    };

    server.router().add_get_handler(
        "/hello", plain_text_handler([](auto& req, auto& httpfunc) {
            return "<B>Hello World</B>";
        }));
    server.router().add_get_handler(
        "/infinite", plain_text_handler([](auto& req, auto& httpfunc) {
            std::this_thread::sleep_for(std::chrono::seconds(100));
            return "<B>infinite</B>";
        }));
    DumpUploader uploader(server.router(),root);
    server.start(threadPool);
    return 0;
}
