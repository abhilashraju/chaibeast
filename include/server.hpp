#pragma once
#include "abstractforwarder.hpp"
#include "chai_concepts.hpp"
#include "handle_error.hpp"
#include "ssl_steam_maker.hpp"

#include <exec/single_thread_context.hpp>
#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <iostream>

namespace chai {
struct SocketStreamMaker {

  struct SocketStreamReader {
    tcp::socket stream;
    auto read() {
      beast::flat_buffer buffer;
      http::request<http::dynamic_body> request;
      http::read(stream, buffer, request);
      return request;
    }
    void close(beast::error_code &ec) { stream.close(); }
  };
  SocketStreamReader acceptConnection(net::io_context &ioContext,
                                      tcp::acceptor &acceptor) {
    tcp::socket clientSocket(ioContext);
    acceptor.accept(clientSocket);
    return SocketStreamReader{std::move(clientSocket)};
  }
};
struct SslStreamMaker {
  struct SSlStreamReader {
    ssl::stream<tcp::socket> stream;
    auto read() {
      beast::flat_buffer buffer;
      http::request<http::dynamic_body> request;
      http::read(stream, buffer, request);
      return request;
    }
    void close(beast::error_code &ec) {
      stream.shutdown(ec);
      stream.next_layer().close();
    }
  };
  ssl::context sslContext;
  SslStreamMaker(const char *pemFile, const char *privKey,
                 const char *certsDirectory)
      : sslContext(getSslServerContext(pemFile, privKey, certsDirectory)) {}
  SSlStreamReader acceptConnection(net::io_context &ioContext,
                                   tcp::acceptor &acceptor) {

    ssl::stream<tcp::socket> clientSocket(ioContext, sslContext);
    acceptor.accept(clientSocket.next_layer());
    clientSocket.handshake(ssl::stream_base::server);
    return SSlStreamReader{std::move(clientSocket)};
  }
};

template <typename Stream> inline auto handleRead(Stream &sock) {
  return stdexec::just() | stdexec::then([&]() { return sock.read(); });
}

inline auto processRequest() {
  return stdexec::then([&](auto handlerRequestPair) {
    auto &[handler, request] = handlerRequestPair;
    return (*handler)(request);
  });
}
struct processError {
  ChaiResponse operator()(auto &e) const {
    std::cout << "Internal Server Error \n" << e.what() << "\n";
    http::response<http::string_body> resp;
    resp.body() = std::string("Internal Server error ") + e.what();
    return resp;
  }
};
inline auto sendResponse(auto &clientSocket) {
  return stdexec::then([&](auto resVar) {
    return std::visit(
        [&](auto &&res) {
          http::write(clientSocket, std::forward<decltype(res)>(res));
        },
        resVar);
  });
}
inline auto selectForwarder(auto &router) {
  return stdexec::then([&router](auto request) {
    return std::make_pair(router.get(request.target()), request);
  });
}
inline void handleClientRequest(auto clientSocket, auto &router) {

  auto work = handleRead(clientSocket) | selectForwarder(router) |
              processRequest() | handle_error(processError{}) |
              sendResponse(clientSocket.stream);

  stdexec::sync_wait(work);
  beast::error_code ec{};
  clientSocket.close(ec);
}

inline auto makeAcceptor(auto &ioContext, auto &endpoint) {
  return stdexec::just() | stdexec::then([&]() {
           tcp::acceptor acceptor(ioContext, endpoint);
           acceptor.listen();
           std::cout << "Proxy server started. Listening on port "
                     << endpoint.port() << "\n";
           return acceptor;
         });
}
inline auto makeConnectionHandler(auto clientSock, auto &router) {
  return stdexec::just(std::move(clientSock)) |
         stdexec::then([&router](auto clientSock) {
           handleClientRequest(std::move(clientSock), router);
         });
}
inline auto waitForConnection(auto &ioContext, auto &pool, auto &router,
                              auto &streamMaker) {
  return stdexec::then([&](auto acceptor) {
    while (true) {
      try {
        auto stream = streamMaker.acceptConnection(ioContext, acceptor);

        auto work = makeConnectionHandler(std::move(stream), router);
        auto snd = stdexec::on(pool.get_scheduler(), std::move(work));
        stdexec::start_detached(std::move(snd));

      } catch (const std::exception &e) {
        std::cout << e.what() << '\n';
      }
    }
    return 0;
  });
}
template <typename Demultiplexer, typename StreamMaker> struct Server {
  std::string port;
  Demultiplexer &router;
  StreamMaker streamMaker;
  Server(std::string_view p, Demultiplexer &dmult, StreamMaker streamMaker)
      : port(p.data(), p.length()), router(dmult),
        streamMaker(std::move(streamMaker)) {}

  void start(auto &clientThreadPool) {
    net::io_context ioContext;
    tcp::endpoint endpoint(tcp::v4(), std::atoi(port.data()));

    auto server =
        makeAcceptor(ioContext, endpoint) |
        waitForConnection(ioContext, clientThreadPool, router, streamMaker);
    std::cout << std::get<0>(stdexec::sync_wait(server).value());
  }
};
template <typename Demultiplexer>
struct SSlServer : Server<Demultiplexer, SslStreamMaker> {
  using Base = Server<Demultiplexer, SslStreamMaker>;
  SSlServer(std::string_view p, Demultiplexer &router, const char *cert,
            const char *privKey,
            const char *certstore = "/etc/ssl/certs/authority")
      : Base(p, router, SslStreamMaker(cert, privKey, certstore)) {}
};
} // namespace chai