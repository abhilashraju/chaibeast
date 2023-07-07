#include "regex_range.hpp"
#include "server.hpp"

#include <string>
namespace chai
{
struct request_mapper
{
    std::string path;
    http::verb method;
    friend bool operator==(const request_mapper& first,
                           const request_mapper& second)
    {
        if (first.method != second.method)
            return false;
        int split_count{0};
        std::string incomingurl = second.path;
        for (auto str : regex_range{first.path, std::regex(R"(\{(.*?)\})")}
                            .get_unmatched())
        {
            auto index = incomingurl.find(str);
            if (index == std::string::npos)
            {
                return false;
            }
            incomingurl = incomingurl.substr(index + str.length());
            split_count++;
        }
        if (split_count == 1)
        { // if not decorated path with {some data} syntax,
          // return exact match
            return first.path == second.path;
        }
        return incomingurl.empty();
    }
    friend bool operator!=(const request_mapper& first,
                           const request_mapper& second)
    {
        return !(first == second);
    }
    friend bool operator<(const request_mapper& first,
                          const request_mapper& second)
    {
        return first.path < second.path;
    }
};

} // namespace chai
