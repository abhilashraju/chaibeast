#pragma once
#include "common_defs.hpp"
#include "http_target_parser.hpp"

#include <concepts>
namespace chai
{
template <typename Handler>
concept DynBodyRequestHandler =
    requires(Handler h, const DynamicbodyRequest& r, const http_function& f) {
        {
            h(r, f)
        } -> std::same_as<VariantResponse>;
    };

} // namespace chai
