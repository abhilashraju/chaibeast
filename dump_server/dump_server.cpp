#include "command_line_parser.hpp"
#include "http_server.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utilities.hpp>

using namespace chai;
constexpr const char *trustStorePath = "/etc/ssl/certs/authority";
constexpr const char *x509Comment = "Generated from OpenBMC service";

int main(int argc, const char *argv[]) {
  // Create a server endpoint
  auto [port] = getArgs(parseCommandline(argc, argv), "-p");
  if (port.empty()) {
    std::cout << "Invalid arguments\n";
    std::cout << "eg: fileserver -p port\n";
    return 0;
  }
  exec::single_thread_context threadPool;
  HttpServer server(
      port, "/Users/abhilashraju/work/cpp/chai/certs/server-certificate.pem",
      "/Users/abhilashraju/work/cpp/chai/certs/server-private-key.pem",
      "/etc/ssl/certs/authority");

  auto plain_text_handler = [](auto func) {
    return [func = std::move(func)](auto &req, auto &httpfunc) {
      http::response<http::string_body> resp{http::status::ok, version(req)};
      resp.set(http::field::content_type, "text/plain");
      resp.body() = func(req, httpfunc);
      resp.set(http::field::content_length,
               std::to_string(resp.body().length()));
      return resp;
    };
  };
  server.router().add_get_handler(
      "/hello", plain_text_handler([](auto &req, auto &httpfunc) {
        std::cout << "reached hello handler";
        return "<B>Hello World</B>";
      }));
  server.start(threadPool);
  return 0;
}
