#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
namespace chai {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

using ChaiResponse = std::variant<http::response<http::file_body>,
                                  http::response<http::string_body>,
                                  http::response<http::dynamic_body>>;
using ChaiRequest = std::variant<http::request<http::file_body>,
                                 http::request<http::string_body>,
                                 http::request<http::dynamic_body>>;
inline auto target(ChaiRequest &reqVariant) {
  return std::visit([](auto &&req) { return req.target(); }, reqVariant);
}
inline http::verb method(ChaiRequest &reqVariant) {
  return std::visit([](auto &&req) { return req.method(); }, reqVariant);
}
inline auto version(ChaiRequest &reqVariant) {
  return std::visit([](auto &&req) { return req.version(); }, reqVariant);
}
} // namespace chai