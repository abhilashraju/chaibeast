#pragma once
#include "common_defs.hpp"
namespace chai
{
struct AbstractForwarder
{
    virtual VariantResponse operator()(VariantRequest& request) const = 0;
    virtual ~AbstractForwarder(){};
};
struct HeaderReader
{
    static auto readHeader(auto& stream, beast::flat_buffer& buffer,
                           beast::error_code& ec)
    {
        http::response_parser<http::empty_body> p;
        // Inform the parser that there will be no body.
        p.skip(true);
        http::read_header(stream, buffer, p, ec);
        return p.release();
    }
};
struct DynamicBodyReader : HeaderReader
{
    static auto read(auto& stream, beast::flat_buffer& serverBuffer,
                     beast::error_code& ec)
    {
        http::response<http::dynamic_body> res{};
        http::read(stream, serverBuffer, res, ec);
        return res;
    }
};
struct IncrementalReader : HeaderReader
{
    static auto read(auto& stream, beast::error_code& ec)
    {
        //     beast::flat_buffer serverBuffer;
        //     http::parser<isRequest, http::buffer_body> p;
        //     http::read_header(stream, serverBuffer, p, ec);
        //     if (ec)
        //         return;
        //     while (!p.is_done())
        //     {
        //         char buf[512];
        //         p.get().body().data = buf;
        //         p.get().body().size = sizeof(buf);
        //         read(stream, buffer, p, ec);
        //         if (ec == error::need_buffer)
        //             ec.assign(0, ec.category());
        //         if (ec)
        //             return;
        //         os.write(buf, sizeof(buf) - p.get().body().size);
        //     }
        // }
        return 0;
    }
};

struct FileBodyReader : HeaderReader
{
    static auto read(auto& stream, beast::flat_buffer& serverBuffer,
                     beast::error_code& ec)
    {
        http::file_body::value_type file;
        file.open("tmp", beast::file_mode::write, ec);
        http::response<http::file_body> res{};
        // std::piecewise_construct,

        // std::make_tuple(std::move(res0), std::move(file))};
        res.body() = std::move(file);
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
    auto checkFail(beast::error_code& ec) const
    {
        if (ec)
        {
            throw std::runtime_error("Error response from end point " +
                                     ec.message());
        }
    };
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

        auto returnResp =
            [this, &forwarderStream](auto&& resp, beast::error_code& ec) {
            checkFail(ec);
            streamBuilder.endStream(forwarderStream, ec);
            return resp;
        };
        beast::flat_buffer buffer;
        return returnResp(BodyReader::read(forwarderStream, buffer, ec), ec);
        // Receive the response from the target server
    }
};

template <typename StreamBuilder>
struct CommonForwarderImpl : AbstractForwarder
{
    std::string target;
    std::string port;
    StreamBuilder streamBuilder;
    using STREAM_TYPE = decltype(streamBuilder.beginStream(
        std::declval<net::io_context&>(), target, port));
    ~CommonForwarderImpl() {}
    CommonForwarderImpl(const std::string& t, const std::string& p) :
        target(t), port(p)
    {}
    auto checkFail(beast::error_code& ec) const
    {
        if (ec)
        {
            throw std::runtime_error("Error response from end point " +
                                     ec.message());
        }
    };
    VariantResponse readAsFile(STREAM_TYPE& stream, beast::flat_buffer& buffer,
                               http::response_parser<http::empty_body>& res0,
                               auto& ec) const
    {
        http::response_parser<http::file_body> res{std::move(res0)};
        http::file_body::value_type file;
        file.open("tmp", beast::file_mode::write, ec);
        checkFail(ec);
        res.get().body() = std::move(file);
        http::read(stream, buffer, res);
        res.get().body().seek(0, ec);
        res.get().prepare_payload();
        checkFail(ec);
        return res.release();
    }
    VariantResponse read(STREAM_TYPE& stream, beast::flat_buffer& buffer,
                         beast::error_code& ec) const

    {
        // Start with an empty_body parser
        http::response_parser<http::empty_body> res0;
        // Read just the header. Otherwise, the empty_body
        // would generate an error if body octets were received.
        // res0.skip(true);
        http::read_header(stream, buffer, res0, ec);
        checkFail(ec);

        constexpr int Threshold = 8000;
        if (res0.content_length())
        {
            if (res0.content_length().value() > Threshold)
            {
                return readAsFile(stream, buffer, res0, ec);
            }
            http::response_parser<http::dynamic_body> res{std::move(res0)};
            // Finish reading the message
            http::read(stream, buffer, res, ec);
            res.get().prepare_payload();
            checkFail(ec);
            return res.release();
        }
        return res0.release();
    }
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

        auto returnResp =
            [this, &forwarderStream](auto&& resp, beast::error_code& ec) {
            checkFail(ec);
            streamBuilder.endStream(forwarderStream, ec);
            return resp;
        };
        beast::flat_buffer buffer;
        return returnResp(read(forwarderStream, buffer, ec), ec);
        // Receive the response from the target server
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
using SecureFlexibleForwarder = CommonForwarderImpl<SSlStreamBuilder>;
#endif
using GenericForwarder =
    GenericForwarderImpl<TcpSocketBuilder, DynamicBodyReader>;

using FileRequestForwarder =
    GenericForwarderImpl<TcpSocketBuilder, FileBodyReader>;
using FlexibleForwarder = CommonForwarderImpl<TcpSocketBuilder>;

} // namespace chai
