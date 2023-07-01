#pragma once
#include "abstractforwarder.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <unordered_map>
namespace chai {
using FORWARD_MAKER = std::unique_ptr<AbstractForwarder> (*)(
    const std::string &, const std::string &);
using MAKER_FACTORY = std::map<std::string, FORWARD_MAKER>;
inline std::unique_ptr<AbstractForwarder>
file_forwarder(const std::string &ip, const std::string &port) {
  return std::make_unique<FileRequestForwarder>(ip, port);
}
inline std::unique_ptr<AbstractForwarder>
generic_forwarder(const std::string &ip, const std::string &port) {
  return std::make_unique<GenericForwarder>(ip, port);
}
inline std::unique_ptr<AbstractForwarder>
secure_file_forwarder(const std::string &ip, const std::string &port) {
  return std::make_unique<SecureFileRequestForwarder>(ip, port);
}
inline std::unique_ptr<AbstractForwarder>
secure_generic_forwarder(const std::string &ip, const std::string &port) {
  return std::make_unique<SecureGenericForwarder>(ip, port);
}
static MAKER_FACTORY &forwardMaker() {
  static MAKER_FACTORY gmakerFactory;
  if (gmakerFactory.empty()) {
    gmakerFactory["file_forwarder"] = file_forwarder;
    gmakerFactory["secure_file_forwarder"] = secure_file_forwarder;
    gmakerFactory["generic_forwarder"] = generic_forwarder;
    gmakerFactory["secure_generic_forwarder"] = secure_generic_forwarder;
  }
  return gmakerFactory;
}
struct Router {
  using Map =
      std::unordered_map<std::string, std::unique_ptr<AbstractForwarder>>;
  Map table;
  std::unique_ptr<AbstractForwarder> defaultForwarder;
  Router(const std::string &config) {

    std::ifstream i(config);
    nlohmann::json routdef;
    i >> routdef;
    for (auto &r : routdef) {

      auto makeForwader = forwardMaker()[r["type"].get<std::string>()];
      if (makeForwader) {
        std::cout << r["path"].get<std::string>();
        addRoute(r["path"].get<std::string>(),
                 std::move(makeForwader(r["ip"].get<std::string>(),
                                        r["port"].get<std::string>())));
        if (!r["default"].is_null() && r["default"].get<bool>()) {
          defaultForwarder.reset(makeForwader(r["ip"].get<std::string>(),
                                              r["port"].get<std::string>())
                                     .release());
        }
      }
    }
  }
  void addRoute(const std::string &path,
                std::unique_ptr<AbstractForwarder> route) {
    table[path] = std::move(route);
  }
  AbstractForwarder *get(const std::string &path) const {
    if (const auto &iter = table.find(path); iter != end(table)) {
      return iter->second.get();
    }
    return defaultForwarder.get();
  }
};
} // namespace chai
