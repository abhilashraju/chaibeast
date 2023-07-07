#pragma once
#include "common_defs.hpp"
#include "http_target_parser.hpp"
namespace redfish
{

inline chai::VariantResponse processUpload(const chai::DynamicbodyRequest& req,
                                           const chai::http_function& httpfunc)
{
    chai::StringbodyResponse res{chai::http::status::ok, req.version()};
    res.body() = "File requested" + httpfunc["filename"];
    return res;
}

} // namespace redfish
