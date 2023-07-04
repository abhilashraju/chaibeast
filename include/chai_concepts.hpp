#pragma once
#include <concepts>
namespace chai {
template <typename Handler, typename Type>
concept ReadHandler = requires(Handler h, Type &in) {
  { h(in) } -> std::same_as<http::request<http::dynamic_body>>;
};

} // namespace chai