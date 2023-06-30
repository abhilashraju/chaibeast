#include "command_line_parser.hpp"
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace chai;
int main(int argc, const char *argv[]) {
  // Create a server endpoint
  auto [port] = getArgs(parseCommandline(argc, argv), "-p");
  if (port.empty()) {
    std::cout << "Invalid arguments\n";
    std::cout << "eg: fileserver -p port\n";
    return 0;
  }
  net::io_context ioc;
  tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), atoi(port.data())));

  while (true) {
    // Wait for a client to connect
    try {
      tcp::socket socket(ioc);
      acceptor.accept(socket);
      auto deleter = [](auto soc) {
        soc->shutdown(tcp::socket::shutdown_send);
      };
      std::unique_ptr<tcp::socket, decltype(deleter)> ptr(&socket, deleter);
      // Read request
      beast::flat_buffer buffer;
      http::request<http::dynamic_body> req;
      http::read(socket, buffer, req);

      // Prepare the response
      http::response<http::file_body> response;

      // Set the content type based on the file extension
      std::string extension = ".html";
      response.set(http::field::content_type, "text/html");

      auto const not_found = [&req](beast::string_view target) {
        http::response<http::string_body> res{http::status::not_found,
                                              req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() =
            "The resource '" + std::string(target) + "' was not found.";
        res.prepare_payload();
        return res;
      };

      // Returns a server error response
      auto const server_error = [&req](beast::string_view what) {
        http::response<http::string_body> res{
            http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + std::string(what) + "'";
        res.prepare_payload();
        return res;
      };
      // Open the file
      beast::error_code ec;
      http::file_body::value_type body;
      std::string filetofetch{req.target().substr(1).data(),
                              req.target().length() - 1};
      filetofetch = std::filesystem::path("/tmp").c_str() + std::string("/") +
                    filetofetch;
      body.open(filetofetch.data(), beast::file_mode::scan, ec);

      // Handle the case where the file doesn't exist
      if (ec == beast::errc::no_such_file_or_directory) {
        http::write(socket, not_found(req.target()));
        continue;
      }

      // Handle an unknown error
      if (ec) {
        http::write(socket, server_error(ec.message()));
        continue;
      }

      // Cache the size since we need it after the move
      auto const size = body.size();

      // Respond to GET request
      http::response<http::file_body> res{
          std::piecewise_construct, std::make_tuple(std::move(body)),
          std::make_tuple(http::status::ok, req.version())};
      res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
      res.set(http::field::content_type, "text/plain");
      res.content_length(size);
      res.keep_alive(req.keep_alive());

      // Send the response
      res.prepare_payload();
      http::write(socket, res);
    } catch (std::exception &e) {
      std::cout << e.what();
    }
  }

  return 0;
}
