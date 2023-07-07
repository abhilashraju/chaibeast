#pragma once
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
namespace chai
{

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

using FilebodyResponse = http::response<http::file_body>;
using StringbodyResponse = http::response<http::string_body>;
using DynamicbodyResponse = http::response<http::dynamic_body>;

using VariantResponse =
    std::variant<FilebodyResponse, StringbodyResponse, DynamicbodyResponse>;
using FilebodyRequest = http::request<http::file_body>;
using StringbodyRequest = http::request<http::string_body>;
using DynamicbodyRequest = http::request<http::dynamic_body>;
using VariantRequest =
    std::variant<FilebodyRequest, StringbodyRequest, DynamicbodyRequest>;

inline auto target(VariantRequest& reqVariant)
{
    return std::visit([](auto&& req) { return req.target(); }, reqVariant);
}
inline http::verb method(VariantRequest& reqVariant)
{
    return std::visit([](auto&& req) { return req.method(); }, reqVariant);
}
inline auto version(VariantRequest& reqVariant)
{
    return std::visit([](auto&& req) { return req.version(); }, reqVariant);
}
} // namespace chai
