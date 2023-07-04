#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/container/flat_map.hpp>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
namespace chai {
struct WebServer {
  struct request_mapper {
    std::string path;
    http::verb method;
    friend bool operator==(const request_mapper &first,
                           const request_mapper &second) {
      if (first.method != second.method)
        return false;
      int split_count{0};
      for (auto str : regex_range{first.path, std::regex(R"(\{(.*?)\})")}
                          .get_unmatched()) {
        if (second.path.find(str) == std::string::npos) {
          return false;
        }
        split_count++;
      }
      if (split_count == 1) { // if not decorated path with {some data} syntax,
                              // return exact match
        return first.path == second.path;
      }
      return true;
    }
    friend bool operator!=(const request_mapper &first,
                           const request_mapper &second) {
      return !(first == second);
    }
    friend bool operator<(const request_mapper &first,
                          const request_mapper &second) {
      return first.path < second.path;
    }
  };

  struct handler_base {
    using ReqVariant = std::variant<http::request<http::dynamic_body>,
                                    http::request<http::file_body>>;
    using ResVariant = std::variant<http::response<http::dynamic_body>,
                                    http::response<http::file_body>>;
    virtual ResVariant handle(ReqVariant &req, const http_function &vw) = 0;
    virtual ~handler_base() {}
  };

  template <typename HandlerFunc> struct handler : handler_base {
    HandlerFunc func;
    handler(HandlerFunc fun) : func(std::move(fun)) {}

    http::response<http::string_body>
    handle(ReqVariant &req, const http_function &params) override {
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

  auto &handler_for_verb(http::verb v) {
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

  auto process_request(auto &req) {
    auto httpfunc = bingo::parse_function(to_ssv(req.target()));
    auto &handlers = handler_for_verb(req.method());
    if (auto iter = handlers.find({httpfunc.name(), req.method()});
        iter != std::end(handlers)) {
      extract_params_from_path(httpfunc, iter->first.path, httpfunc.name());
      return iter->second->handle(req, httpfunc);
    }
    throw file_not_found(httpfunc.name());
  }
  flat_map<request_mapper, std::unique_ptr<handler_base>> get_handlers;
  flat_map<request_mapper, std::unique_ptr<handler_base>> post_handlers;
  flat_map<request_mapper, std::unique_ptr<handler_base>> delete_handlers;
};

} // namespace chai