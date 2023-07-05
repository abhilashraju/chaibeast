#include "server.hpp"

#include "regex_range.hpp"
#include <string>
namespace chai {
struct request_mapper {
  std::string path;
  http::verb method;
  friend bool operator==(const request_mapper &first,
                         const request_mapper &second) {
    if (first.method != second.method)
      return false;
    int split_count{0};
    for (auto str :
         regex_range{first.path, std::regex(R"(\{(.*?)\})")}.get_unmatched()) {
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

} // namespace chai