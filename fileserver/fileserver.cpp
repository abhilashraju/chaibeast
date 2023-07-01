#include "command_line_parser.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utilities.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;
using namespace chai;
constexpr const char *trustStorePath = "/etc/ssl/certs/authority";
constexpr const char *x509Comment = "Generated from OpenBMC service";
inline void getSslContext(const std::string &sslPemFile,
                          ssl::context *mSslContext) {
  mSslContext->set_options(boost::asio::ssl::context::default_workarounds |
                           boost::asio::ssl::context::no_sslv2 |
                           boost::asio::ssl::context::no_sslv3 |
                           boost::asio::ssl::context::single_dh_use |
                           boost::asio::ssl::context::no_tlsv1 |
                           boost::asio::ssl::context::no_tlsv1_1);

  // BIG WARNING: This needs to stay disabled, as there will always be
  // unauthenticated endpoints
  // mSslContext->set_verify_mode(boost::asio::ssl::verify_peer);

  SSL_CTX_set_options(mSslContext->native_handle(), SSL_OP_NO_RENEGOTIATION);

  std::cout << "Using default TrustStore location: " << trustStorePath;
  mSslContext->add_verify_path(trustStorePath);

  mSslContext->use_certificate_file(sslPemFile, boost::asio::ssl::context::pem);
  mSslContext->use_private_key_file(sslPemFile, boost::asio::ssl::context::pem);

  // Set up EC curves to auto (boost asio doesn't have a method for this)
  // There is a pull request to add this.  Once this is included in an asio
  // drop, use the right way
  // http://stackoverflow.com/questions/18929049/boost-asio-with-ecdsa-certificate-issue
  if (SSL_CTX_set_ecdh_auto(mSslContext->native_handle(), 1) != 1) {
    std::cout << "Error setting tmp ecdh list\n";
  }

  // Mozilla intermediate cipher suites v5.7
  // Sourced from: https://ssl-config.mozilla.org/guidelines/5.7.json
  const char *mozillaIntermediate = "ECDHE-ECDSA-AES128-GCM-SHA256:"
                                    "ECDHE-RSA-AES128-GCM-SHA256:"
                                    "ECDHE-ECDSA-AES256-GCM-SHA384:"
                                    "ECDHE-RSA-AES256-GCM-SHA384:"
                                    "ECDHE-ECDSA-CHACHA20-POLY1305:"
                                    "ECDHE-RSA-CHACHA20-POLY1305:"
                                    "DHE-RSA-AES128-GCM-SHA256:"
                                    "DHE-RSA-AES256-GCM-SHA384:"
                                    "DHE-RSA-CHACHA20-POLY1305";

  if (SSL_CTX_set_cipher_list(mSslContext->native_handle(),
                              mozillaIntermediate) != 1) {
    std::cout << "Error setting cipher list\n";
  }
}
int main(int argc, const char *argv[]) {
  // Create a server endpoint
  auto [port] = getArgs(parseCommandline(argc, argv), "-p");
  if (port.empty()) {
    std::cout << "Invalid arguments\n";
    std::cout << "eg: fileserver -p port\n";
    return 0;
  }
  net::io_context ioc;
  ssl::context sslContext(boost::asio::ssl::context::tls_server);

  getSslContext("/etc/ssl/certs/https/server.pem", &sslContext);

  tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), atoi(port.data())));
  acceptor.listen();
  while (true) {
    // Wait for a client to connect
    try {
      ssl::stream<tcp::socket> stream(ioc, sslContext);

      acceptor.accept(stream.next_layer());

      auto deleter = [](auto soc) {
        beast::error_code ec;
        soc->shutdown(ec);
        soc->next_layer().close();
      };
      std::unique_ptr<ssl::stream<tcp::socket>, decltype(deleter)> ptr(&stream,
                                                                       deleter);
      stream.handshake(ssl::stream_base::server);

      // Read request
      beast::flat_buffer buffer;
      http::request<http::dynamic_body> req;
      http::read(stream, buffer, req);

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
      auto substrings = split(filetofetch, '/');

      filetofetch = std::filesystem::path("/tmp").c_str() + std::string("/") +
                    substrings.back();
      body.open(filetofetch.data(), beast::file_mode::scan, ec);

      // Handle the case where the file doesn't exist
      if (ec == beast::errc::no_such_file_or_directory) {
        http::write(stream, not_found(req.target()));
        continue;
      }

      // Handle an unknown error
      if (ec) {
        http::write(stream, server_error(ec.message()));
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
      http::write(stream, res);
    } catch (std::exception &e) {
      std::cout << e.what();
    }
  }

  return 0;
}
