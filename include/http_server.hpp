#pragma once
#include "http_errors.hpp"
#include "http_target_parser.hpp"
#include "request_mapper.hpp"
#include "server.hpp"
#include <boost/container/flat_map.hpp>
namespace chai {

struct HttpRouter {

  struct handler_base {

    virtual ChaiResponse handle(ChaiRequest &req, const http_function &vw) = 0;
    virtual ~handler_base() {}
  };
  using HANDLER_MAP =
      boost::container::flat_map<request_mapper, std::unique_ptr<handler_base>>;
  template <typename HandlerFunc> struct handler : handler_base {
    HandlerFunc func;
    handler(HandlerFunc fun) : func(std::move(fun)) {}

    ChaiResponse handle(ChaiRequest &req,
                        const http_function &params) override {
      return func(req, params);
    }
  };

  template <typename FUNC>
  void add_handler(const request_mapper &mapper, FUNC &&h) {
    auto &handlers = handler_for_verb(mapper.method);
    handlers[mapper] = std::make_unique<handler<FUNC>>(std::move(h));
  }
  template <typename FUNC>
  void add_get_handler(std::string_view path, FUNC &&h) {
    add_handler({{path.data(), path.length()}, http::verb::get}, (FUNC &&)h);
  }
  template <typename FUNC>
  void add_post_handler(std::string_view path, FUNC &&h) {
    add_handler({{path.data(), path.length()}, http::verb::post}, (FUNC &&)h);
  }
  template <typename FUNC>
  void add_put_handler(std::string_view path, FUNC &&h) {
    add_handler({{path.data(), path.length()}, http::verb::put}, (FUNC &&)h);
  }
  template <typename FUNC>
  void add_delete_handler(std::string_view path, FUNC &&h) {
    add_handler({{path.data(), path.length()}, http::verb::delete_}, path,
                (FUNC &&)h);
  }

  HANDLER_MAP &handler_for_verb(http::verb v) {
    switch (v) {
    case http::verb::get:
      return get_handlers;
    case http::verb::put:
    case http::verb::post:
      return post_handlers;
    case http::verb::delete_:
      return delete_handlers;
    }
    return get_handlers;
  }

  auto process_request(auto &reqVariant) {
    auto httpfunc = parse_function(target(reqVariant));
    auto &handlers = handler_for_verb(method(reqVariant));
    if (auto iter = handlers.find({httpfunc.name(), method(reqVariant)});
        iter != std::end(handlers)) {
      extract_params_from_path(httpfunc, iter->first.path, httpfunc.name());
      return iter->second->handle(reqVariant, httpfunc);
    }
    std::cout << "reached not found";
    throw file_not_found(httpfunc.name());
  }
  ChaiResponse operator()(ChaiRequest &req) { return process_request(req); }

  HttpRouter *get(const std::string &path) { return this; }

  HANDLER_MAP get_handlers;
  HANDLER_MAP post_handlers;
  HANDLER_MAP delete_handlers;
};

struct HttpsServer {
  HttpRouter router_;
  SSlServer<HttpRouter> server;
  HttpsServer(std::string_view port, const char *serverCert,
              const char *serverPkey, const char *trustStore)
      : server(port, router_, serverCert, serverPkey, trustStore) {}
  HttpRouter &router() { return router_; }
  void start(auto &pool) { server.start(pool); }
};
struct HttpServer {
  HttpRouter router_;
  TCPServer<HttpRouter> server;
  HttpServer(std::string_view port) : server(port, router_) {}
  HttpRouter &router() { return router_; }
  void start(auto &pool) { server.start(pool); }
};
} // namespace chai