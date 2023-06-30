#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
namespace chai
{
struct AbstractForwarder
{
    virtual beast::error_code
        operator()(tcp::socket& clientSocket,
                   http::request<http::dynamic_body>& request) const = 0;
    virtual ~AbstractForwarder(){};
};
struct GenericForwarder : AbstractForwarder
{
    std::string target;
    std::string port;
    ~GenericForwarder() {}
    GenericForwarder(const std::string& t, const std::string& p) :
        target(t), port(p)
    {}
    beast::error_code
        operator()(tcp::socket& clientSocket,
                   http::request<http::dynamic_body>& request) const
    {
        net::io_context ioContext;
        tcp::socket serverSocket(ioContext);
        tcp::resolver resolver(ioContext);
        const auto results = resolver.resolve(target.data(), port.data());
        net::connect(serverSocket, results.begin(), results.end());

        // Forward the request to the target server
        http::write(serverSocket, request);

        // Receive the response from the target server
        beast::flat_buffer serverBuffer;
        http::file_body::value_type file;
        beast::error_code ec;

        http::response<http::dynamic_body> res{};
        http::read(serverSocket, serverBuffer, res, ec);
        if (ec)
        {
            // http::write(clientSocket, tresp, ec);
            return ec;
        }
        http::write(clientSocket, res, ec);
        return ec;
    }
};
struct FileRequestForwarder : AbstractForwarder
{
    std::string target;
    std::string port;
    ~FileRequestForwarder() {}
    FileRequestForwarder(const std::string& t, const std::string& p) :

        target(t), port(p)
    {}
    beast::error_code
        operator()(tcp::socket& clientSocket,
                   http::request<http::dynamic_body>& request) const
    {
        net::io_context ioContext;
        tcp::socket serverSocket(ioContext);
        tcp::resolver resolver(ioContext);
        const auto results = resolver.resolve(target.data(), port.data());
        net::connect(serverSocket, results.begin(), results.end());

        // Forward the request to the target server
        http::write(serverSocket, request);

        // Receive the response from the target server
        beast::flat_buffer serverBuffer;
        http::file_body::value_type file;
        beast::error_code ec;

        file.open("tmp", beast::file_mode::write, ec);
        if (ec)
        {
            return ec;
        }

        http::response<http::file_body> res{std::piecewise_construct,
                                            std::make_tuple(std::move(file))};
        http::response_parser<http::file_body> parser{std::move(res)};

        parser.body_limit((std::numeric_limits<std::uint64_t>::max)());
        http::read(serverSocket, serverBuffer, parser);

        // Forward the response to the client
        auto tresp = parser.release();
        tresp.body().seek(0, ec);
        if (ec)
        {
            // http::write(clientSocket, tresp, ec);
            return ec;
        }
        http::write(clientSocket, tresp, ec);
        return ec;
    }
};
} // namespace chai