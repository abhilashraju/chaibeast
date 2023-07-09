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
  public:
    DumpUploader(auto& router)
    {
        router.add_get_handler(
            "/redfish/v1/Systems/system/LogServices/Dump/{filename}/Entries",
            std::bind_front(&DumpUploader::processUpload, this));
    }
    chai::VariantResponse processUpload(const chai::DynamicbodyRequest& req,
                                        const chai::http_function& httpfunc)
    {
        chai::http::file_body::value_type body;

        auto filetofetch = std::filesystem::path(
                               "/Users/abhilashraju/work/cpp/chaiproxy/build/")
                               .c_str() +
                           httpfunc["filename"];
        chai::beast::error_code ec{};
        body.open(filetofetch.data(), chai::beast::file_mode::scan, ec);
        if (ec == chai::beast::errc::no_such_file_or_directory)
        {
            throw chai::file_not_found(httpfunc["filename"]);
        }
        const auto size = body.size();

        // Respond to GET request
        chai::http::response<chai::http::file_body> res{
            std::piecewise_construct, std::make_tuple(std::move(body)),
            std::make_tuple(http::status::ok, req.version())};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/plain");
        res.content_length(size);
        res.prepare_payload();
        return res;
    }
};

} // namespace redfish
