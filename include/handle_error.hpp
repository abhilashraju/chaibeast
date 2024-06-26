#pragma once
#include <stdexec/execution.hpp>
namespace chai
{
template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
inline auto handle_error(auto handler)
{
    return stdexec::let_error([=](auto exptr) {
        // try {
        //   std::rethrow_exception(exptr);
        // } catch (const std::exception &e) {
        //   std::variant<std::exception> v(e);
        //   return stdexec::just(std::visit(overloaded{handlers...}, v));
        // }
        return stdexec::just(exptr) |
               stdexec::then([handler = std::move(handler)](auto exptr) {
            try
            {
                std::rethrow_exception(exptr);
            }
            catch (const std::exception& e)
            {
                return handler(e);
            }
        });
    });
}
} // namespace chai
