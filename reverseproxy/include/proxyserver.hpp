#pragma once
#include "abstractforwarder.hpp"
#include "router.hpp"

#include <exec/single_thread_context.hpp>
#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <iostream>

namespace chai {
inline auto readHandler(tcp::socket &sock) {
  beast::flat_buffer buffer;
  http::request<http::dynamic_body> request;
  http::read(sock, buffer, request);
  return request;
}

template <typename ReadHandler>
inline auto handleRead(tcp::socket &sock, ReadHandler readHandler) {
  return stdexec::just() |
         stdexec::then(
             [&, handler = std::move(readHandler)]() { return handler(sock); });
}

inline auto processRequest(tcp::socket &clientSocket) {
  return stdexec::then([&](auto handlerRequestPair) {
    auto &[handler, request] = handlerRequestPair;
    return (*handler)(clientSocket, request);
  });
}
inline auto selectForwarder(auto &router) {
  return stdexec::then([&router](auto request) {
    return std::make_pair(router.get(request.target()), request);
  });
}
inline void handleClientRequest(tcp::socket clientSocket, Router &router) {
  try {
    auto work = handleRead(clientSocket, readHandler) |
                selectForwarder(router) | processRequest(clientSocket);

    stdexec::sync_wait(work);
  } catch (std::exception &e) {
    std::cout << "Error " << e.what() << "\n";
  }
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
inline auto makeConnectionHandler(tcp::socket clientSock, Router &router) {
  return stdexec::just(std::move(clientSock)) |
         stdexec::then([&router](auto clientSock) {
           handleClientRequest(std::move(clientSock), router);
         });
}
inline auto waitForConnection(auto &ioContext, auto &pool, Router &router) {
  return stdexec::then([&](auto acceptor) {
    while (true) {
      tcp::socket clientSocket(ioContext);
      acceptor.accept(clientSocket);

      auto work = makeConnectionHandler(std::move(clientSocket), router);
      auto snd = stdexec::on(pool.get_scheduler(), std::move(work));
      stdexec::start_detached(std::move(snd));
      // handleClientRequest(std::move(clientSocket));
    }
    return 0;
  });
}
struct Server {
  std::string port;
  Router router;
  Server(const std::string_view &p, const std::string_view config)
      : port(p.data(), p.length()), router({config.data(), config.length()}) {}

  void start(auto &clientThreadPool) {
    net::io_context ioContext;
    tcp::endpoint endpoint(tcp::v4(), std::atoi(port.data()));

    auto server = makeAcceptor(ioContext, endpoint) |
                  waitForConnection(ioContext, clientThreadPool, router);
    std::cout << std::get<0>(stdexec::sync_wait(server).value());
  }
};
} // namespace chai