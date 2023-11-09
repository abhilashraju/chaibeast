#pragma once
#include "common_defs.hpp"
#include "http_errors.hpp"
#include "http_target_parser.hpp"

#include <filesystem>
#include <functional>
namespace http = chai::http;
namespace beast = chai::beast;
namespace redfish
{
class DumpUploader
{
    std::string_view rootPath;

  public:
    DumpUploader(auto& router, auto root) : rootPath(root)
    {
        router.add_get_handler(
            "/redfish/v1/Systems/system/LogServices/Dump/{filename}/attachment",
            std::bind_front(&DumpUploader::processUpload, this));
        router.add_get_handler(
            "/redfish/v1/Systems/system/LogServices/Dump/Entries",
            std::bind_front(&DumpUploader::entries, this));
    }
    auto fetchFile(const chai::DynamicbodyRequest& req,
                   const std::string& filetofetch)
    {
        chai::http::file_body::value_type body;
        chai::beast::error_code ec{};
        body.open(filetofetch.data(), chai::beast::file_mode::scan, ec);
        if (ec == chai::beast::errc::no_such_file_or_directory)
        {
            throw chai::file_not_found(filetofetch);
        }
        const auto size = body.size();

        // Respond to GET request
        chai::http::response<chai::http::file_body> res{
            std::piecewise_construct, std::make_tuple(std::move(body)),
            std::make_tuple(http::status::ok, req.version())};
        http::response_serializer<http::file_body> parser{res};
        // parser.body_limit((std::numeric_limits<std::uint64_t>::max)());
        parser.get().set(http::field::server, BOOST_BEAST_VERSION_STRING);
        parser.get().set(http::field::content_type, "text/plain");
        parser.get().content_length(size);
        parser.get().prepare_payload();
        return parser;
    }
    auto fetchFileUrl(const chai::DynamicbodyRequest& req,
                      const std::string& filetofetch)
    {
        if (!std::filesystem::exists(filetofetch))
        {
            throw chai::file_not_found(filetofetch);
        }

        // Respond to GET request
        chai::http::response<chai::http::string_body> res{http::status::ok,
                                                          req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.insert("fileurl", filetofetch);
        res.content_length(0);
        res.prepare_payload();
        return res;
    }
    chai::VariantResponse processUpload(const chai::DynamicbodyRequest& req,
                                        const chai::http_function& httpfunc)
    {
        auto filetofetch = std::filesystem::path(rootPath).c_str() +
                           httpfunc["filename"];
        return fetchFileUrl(req, filetofetch);
    }
    chai::VariantResponse entries(const chai::DynamicbodyRequest& req,
                                  const chai::http_function& httpfunc)
    {
        auto filetofetch = std::filesystem::path(rootPath).c_str() +
                           std::string("Entries");
        while (true)
            ;
        return fetchFileUrl(req, filetofetch);
    }
};

} // namespace redfish
