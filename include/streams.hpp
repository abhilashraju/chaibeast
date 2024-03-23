#pragma once
#include "chai_concepts.hpp"
#include "common_defs.hpp"
#include "handle_error.hpp"
#include "ssl_stream_maker.hpp"
namespace chai
{
inline auto checkFailed(beast::error_code& ec)
{
    if (ec)
    {
        std::cout << ec.message() << "\n";
        return true;
    }
    return false;
}
struct TCPStreamMaker
{
    struct SocketStreamReader
    {
        std::unique_ptr<net::io_context> ctx{
            std::make_unique<net::io_context>()};
        tcp::socket stream_{*ctx};
        SocketStreamReader() {}
        SocketStreamReader(const SocketStreamReader&) = delete;
        SocketStreamReader(SocketStreamReader&&) = default;
        // SocketStreamReader(tcp::socket&& s) : stream(std::move(s)) {}
        auto read(beast::error_code& ec)
        {
            beast::flat_buffer buffer;
            StringbodyRequest request;
            http::read(stream_, buffer, request, ec);
            return VariantRequest{std::move(request)};
        }
        auto read()
        {
            beast::error_code ec{};
            auto ret = read(ec);
            if (ec)
            {
                throw std::runtime_error(ec.message());
            }
            return ret;
        }
        void close(beast::error_code& ec)
        {
            stream_.close();
        }
        tcp::socket& stream()
        {
            return stream_;
        }
    };
    SocketStreamReader acceptConnection(net::io_context& ioContext,
                                        tcp::acceptor& acceptor)
    {
        SocketStreamReader reader;
        acceptor.accept(reader.stream());
        return reader;
    }
    void acceptAsyncConnection(net::io_context& ioContext,
                               tcp::acceptor& acceptor, auto work)
    {
        auto do_accept = [&ioContext, work, this,
                          &acceptor](auto accept, net::yield_context yield) {
            SocketStreamReader reader;
            beast::error_code ec{};
            acceptor.async_accept(reader.stream(), yield);
            if (checkFailed(ec))
            {
                net::spawn(&ioContext, std::bind_front(accept, accept));
                return;
            }

            auto do_work =
                [accept, &ioContext, work,
                 reader = std::move(reader)](net::yield_context yield) mutable {
                work(std::move(reader));
                net::spawn(&ioContext, std::bind_front(accept, accept));
            };
            net::spawn(ioContext, std::move(do_work));
        };

        net::spawn(ioContext, std::bind_front(do_accept, do_accept));
    }
};
#ifdef SSL_ON
struct SslStreamMaker
{
    struct SSlStreamReader
    {
        std::unique_ptr<net::io_context> ctx{
            std::make_unique<net::io_context>()};
        ssl::stream<tcp::socket> stream_;
        SSlStreamReader(const SSlStreamReader&) = delete;
        SSlStreamReader(SSlStreamReader&&) = default;
        SSlStreamReader(net::io_context& ioctx, ssl::context& sslContext) :
            stream_(ioctx, sslContext)
        {}
        auto read(beast::error_code& ec)
        {
            beast::flat_buffer buffer;
            StringbodyRequest request;
            http::read(stream_, buffer, request, ec);
            return VariantRequest{std::move(request)};
        }

        auto read()
        {
            beast::error_code ec{};
            auto ret = read(ec);
            if (ec)
            {
                throw std::runtime_error(ec.message());
            }
            return ret;
        }
        void close(beast::error_code& ec)
        {
            stream_.shutdown(ec);
            stream_.next_layer().close();
        }
        ssl::stream<tcp::socket>& stream()
        {
            return stream_;
        }
    };
    ssl::context sslContext;
    SslStreamMaker(const char* pemFile, const char* privKey,
                   const char* certsDirectory) :
        sslContext(getSslServerContext(pemFile, privKey, certsDirectory))
    {}
    SSlStreamReader acceptConnection(net::io_context& ioContext,
                                     tcp::acceptor& acceptor)
    {
        SSlStreamReader reader(ioContext, sslContext);
        acceptor.accept(reader.stream().next_layer());
        reader.stream().handshake(ssl::stream_base::server);
        return reader;
    }
    void acceptAsyncConnection(net::io_context& ioContext,
                               tcp::acceptor& acceptor, auto work)
    {
        auto do_accept = [&ioContext, work, this,
                          &acceptor](auto accept, net::yield_context yield) {
            SSlStreamReader reader(ioContext, sslContext);
            beast::error_code ec{};
            acceptor.async_accept(reader.stream().next_layer(), yield[ec]);
            if (checkFailed(ec))
            {
                net::spawn(&ioContext, std::bind_front(accept, accept));
                return;
            }
            auto do_handshake =
                [accept, &ioContext, work,
                 reader = std::move(reader)](net::yield_context yield) mutable {
                beast::error_code ec{};
                reader.stream().async_handshake(ssl::stream_base::server,
                                                yield[ec]);
                if (checkFailed(ec))
                {
                    net::spawn(&ioContext, std::bind_front(accept, accept));
                    return;
                }
                auto do_work = [accept, &ioContext, work,
                                reader = std::move(reader)](
                                   net::yield_context yield) mutable {
                    work(std::move(reader));
                    net::spawn(&ioContext, std::bind_front(accept, accept));
                };
                net::spawn(ioContext, std::move(do_work));
            };
            net::spawn(ioContext, std::move(do_handshake));
        };
        net::spawn(ioContext, std::bind_front(do_accept, do_accept));
    }
};
#endif
} // namespace chai
