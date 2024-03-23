#pragma once
#include "streams.hpp"

#include <exec/single_thread_context.hpp>
#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <iostream>

namespace chai
{

template <typename Stream>
inline auto handleRead(Stream& sock)
{
    return stdexec::just() | stdexec::then([&]() { return sock.read(); });
}
template <typename Stream>
inline auto handleFinish(Stream& sock)
{
    return stdexec::then([&]() {
        // beast::error_code ec{};
        // do
        // {
        //     sock.read(ec);
        // } while (!ec);
        beast::error_code ec{};
        sock.close(ec);
    });
}

inline auto processRequest()
{
    return stdexec::then([&](auto handlerRequestPair) {
        auto& [handler, request] = handlerRequestPair;
        if (!handler)
        {
            throw std::runtime_error("Suggested forwarder type is null");
        }
        return (*handler)(std::move(request));
    });
}
struct ProcessApplicationError
{
    VariantResponse operator()(auto& e) const
    {
        http::response<http::string_body> resp{
            http::status::internal_server_error, 10};
        resp.body() = std::string("Internal Server error ") + e.what();
        resp.set(http::field::content_length,
                 std::to_string(resp.body().length()));
        return resp;
    }
};

struct ProcessSystemError
{
    auto operator()(auto& e) const
    {
        std::cout << e.what() << "\n";
    }
};
inline auto sendResponse(auto& clientSocket)
{
    return stdexec::then([&](auto resVar) {
        return std::visit(
            [&](auto&& res) {
            http::write(clientSocket, std::forward<decltype(res)>(res));
        },
            resVar);
    });
}
inline auto selectForwarder(auto& router)
{
    return stdexec::then([&router](auto request) {
        return std::make_pair(router.getForwarder(target(request)),
                              std::move(request));
    });
}
inline void handleClientRequest(auto clientSocket, auto& router)
{
    auto work = handleRead(clientSocket) | selectForwarder(router) |
                processRequest() | handle_error(ProcessApplicationError{}) |
                sendResponse(clientSocket.stream()) |
                handleFinish(clientSocket) | handle_error(ProcessSystemError{});

    stdexec::sync_wait(work);
}

inline auto makeAcceptor(auto& ioContext, auto& endpoint)
{
    return stdexec::just() | stdexec::then([&]() {
        tcp::acceptor acceptor(ioContext, endpoint);
        acceptor.listen();
        std::cout << "Proxy server started. Listening on port "
                  << endpoint.port() << "\n";
        return acceptor;
    });
}
inline auto makeConnectionHandler(auto clientSock, auto& router)
{
    return stdexec::just(std::move(clientSock)) |
           stdexec::then([&router](auto clientSock) {
        handleClientRequest(std::move(clientSock), router);
    });
}
inline auto waitForConnection(auto& ioContext, auto& pool, auto& router,
                              auto& streamMaker)
{
    return stdexec::then([&](auto acceptor) {
        while (true)
        {
            try
            {
                auto stream = streamMaker.acceptConnection(ioContext, acceptor);

                auto work = makeConnectionHandler(std::move(stream), router);
                auto snd = stdexec::on(pool.get_scheduler(), std::move(work));
                stdexec::start_detached(std::move(snd));
            }
            catch (const std::exception& e)
            {
                std::cout << e.what() << '\n';
            }
        }
        return 0;
    });
}
inline auto waitForAsyncConnection(auto& ioContext, auto& pool, auto& router,
                                   auto& streamMaker)
{
    return stdexec::then(
        [&ioContext, &pool, &streamMaker, &router](auto acceptor) {
        try
        {
            auto asyncWork = [&ioContext, &pool, &router,
                              &streamMaker](auto stream) {
                auto work = makeConnectionHandler(std::move(stream), router);
                auto snd = stdexec::on(pool.get_scheduler(), std::move(work));
                stdexec::start_detached(std::move(snd));
                // stdexec::sync_wait(std::move(work));
                // waitForAsyncConnection(ioContext, pool, router, streamMaker);
            };
            streamMaker.acceptAsyncConnection(ioContext, acceptor,
                                              std::move(asyncWork));
        }
        catch (const std::exception& e)
        {
            std::cout << e.what() << '\n';
        }
        ioContext.run();

        return 0;
    });
}
template <typename Demultiplexer, typename StreamMaker>
struct Server
{
    std::string port;
    Demultiplexer& router;
    StreamMaker streamMaker;
    Server(std::string_view p, Demultiplexer& dmult, StreamMaker streamMaker) :
        port(p.data(), p.length()), router(dmult),
        streamMaker(std::move(streamMaker))
    {}

    void start(auto& clientThreadPool)
    {
        net::io_context ioContext;
        tcp::endpoint endpoint(tcp::v4(), std::atoi(port.data()));

        auto server = makeAcceptor(ioContext, endpoint) |
                      waitForAsyncConnection(ioContext, clientThreadPool,
                                             router, streamMaker);
        std::cout << std::get<0>(stdexec::sync_wait(server).value());
    }
};
#ifdef SSL_ON
template <typename Demultiplexer>
struct SSlServer : Server<Demultiplexer, SslStreamMaker>
{
    using Base = Server<Demultiplexer, SslStreamMaker>;
    SSlServer(std::string_view p, Demultiplexer& router, const char* cert,
              const char* privKey,
              const char* certstore = "/etc/ssl/certs/authority") :
        Base(p, router, SslStreamMaker(cert, privKey, certstore))
    {}
};
#endif
template <typename Demultiplexer>
struct TCPServer : Server<Demultiplexer, TCPStreamMaker>
{
    using Base = Server<Demultiplexer, TCPStreamMaker>;
    TCPServer(std::string_view p, Demultiplexer& router) :
        Base(p, router, TCPStreamMaker())
    {}
};
} // namespace chai
