#pragma once
#include "abstractforwarder.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <unordered_map>
namespace chai
{
struct Router
{
    using Map =
        std::unordered_map<std::string, std::unique_ptr<AbstractForwarder>>;
    Map table;
    std::unique_ptr<AbstractForwarder> defaultForwarder;
    Router(const std::string& dip, const std::string& dp,
           const std::string& config)
    {
        defaultForwarder.reset(new GenericForwarder{dip, dp});
        std::ifstream i(config);
        nlohmann::json routdef;
        i >> routdef;
        for (auto& r : routdef)
        {
            std::unique_ptr<AbstractForwarder> forwarder;
            if (r["type"].get<std::string>() == "file_forwarder")
            {
                forwarder.reset(new FileRequestForwarder(
                    r["ip"].get<std::string>(), r["port"].get<std::string>()));
            }
            else
            {
                forwarder.reset(new GenericForwarder(
                    r["ip"].get<std::string>(), r["port"].get<std::string>()));
            }
            addRoute(r["path"].get<std::string>(), std::move(forwarder));
        }
    }
    void addRoute(const std::string& path,
                  std::unique_ptr<AbstractForwarder> route)
    {
        table[path] = std::move(route);
    }
    AbstractForwarder* get(const std::string& path) const
    {
        if (const auto& iter = table.find(path); iter != end(table))
        {
            return iter->second.get();
        }
        return defaultForwarder.get();
    }
};
} // namespace chai
