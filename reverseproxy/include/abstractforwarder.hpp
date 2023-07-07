#pragma once
#include "common_defs.hpp"
namespace chai
{
struct AbstractForwarder
{
    virtual VariantResponse operator()(VariantRequest& request) const = 0;
    virtual ~AbstractForwarder(){};
};

template <typename StreamBuilder, typename BodyReader>
struct GenericForwarderImpl : AbstractForwarder
{
    std::string target;
    std::string port;
    StreamBuilder streamBuilder;
    ~GenericForwarderImpl() {}
    GenericForwarderImpl(const std::string& t, const std::string& p) :
        target(t), port(p)
    {}
    VariantResponse operator()(VariantRequest& requestVar) const
    {
        net::io_context ioContext;
        auto forwarderStream = streamBuilder.beginStream(ioContext, target,
                                                         port);
        std::visit(
            [&](auto&& req) {
            // Forward the request to the target server
            http::write(forwarderStream, req);
            },
            requestVar);

        beast::error_code ec{};
        auto res = BodyReader::read(forwarderStream, ec);
        // Receive the response from the target server
        if (ec)
        {
            throw std::runtime_error("Error response from end point");
        }
        streamBuilder.endStream(forwarderStream, ec);
        return res;
    }
};
struct DynamicBodyReader
{
    static auto read(auto& stream, beast::error_code& ec)
    {
        beast::flat_buffer serverBuffer;
        http::response<http::dynamic_body> res{};
        http::read(stream, serverBuffer, res, ec);
        return res;
    }
};
struct FileBodyReader
{
    static auto read(auto& stream, beast::error_code& ec)
    {
        beast::flat_buffer serverBuffer;
        http::file_body::value_type file;
        file.open("tmp", beast::file_mode::write, ec);
        http::response<http::file_body> res{std::piecewise_construct,
                                            std::make_tuple(std::move(file))};

        if (ec)
        {
            return res;
        }

        http::response_parser<http::file_body> parser{std::move(res)};
        parser.body_limit((std::numeric_limits<std::uint64_t>::max)());
        http::read(stream, serverBuffer, parser);
        // Forward the response to the client
        auto tresp = parser.release();
        tresp.body().seek(0, ec);
        return tresp;
    }
};
struct TcpSocketBuilder
{
    auto beginStream(net::io_context& ioContext, const std::string& target,
                     const std::string& port) const
    {
        tcp::socket forwardingSocket(ioContext);
        tcp::resolver resolver(ioContext);
        const auto results = resolver.resolve(target.data(), port.data());
        net::connect(forwardingSocket, results.begin(), results.end());
        return forwardingSocket;
    }
    auto endStream(auto& forwardingSocket, beast::error_code& ec) const {}
};
namespace ssl = boost::asio::ssl;
#ifdef SSL_ON
struct SSlStreamBuilder
{
    auto beginStream(net::io_context& ioContext, const std::string& target,
                     const std::string& port) const
    {
        tcp::resolver resolver(ioContext);
        const auto results = resolver.resolve(target.data(), port.data());

        ssl::context sslContext(ssl::context::tlsv13_client);
        // Set the SSL verification mode and load root certificates
        sslContext.set_verify_mode(ssl::verify_none);
        // sslContext.load_verify_file("/path/to/certificates.pem");

        ssl::stream<tcp::socket> stream(ioContext, sslContext);
        net::connect(stream.next_layer(), results.begin(), results.end());
        stream.handshake(ssl::stream_base::client);
        return stream;
    }
    auto endStream(auto& stream, beast::error_code& ec) const
    {
        stream.shutdown(ec);
    }
};
using SecureGenericForwarder =
    GenericForwarderImpl<SSlStreamBuilder, DynamicBodyReader>;

using SecureFileRequestForwarder =
    GenericForwarderImpl<SSlStreamBuilder, FileBodyReader>;
#endif
using GenericForwarder =
    GenericForwarderImpl<TcpSocketBuilder, DynamicBodyReader>;

using FileRequestForwarder =
    GenericForwarderImpl<TcpSocketBuilder, FileBodyReader>;

} // namespace chai
