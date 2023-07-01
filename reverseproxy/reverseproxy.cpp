
#include "command_line_parser.hpp"
#include "proxyserver.hpp"
using namespace chai;
int main(int argc, const char *argv[]) {
  auto [p, config] = getArgs(parseCommandline(argc, argv), "-p", "-c");
  if (p.empty()) {
    std::cout << "Invalid Invocation. Argument Missing\n";
    std::cout << "Please use\n reverseproxy -p port -c config.json_path \n";
    return 0;
  }
  exec::single_thread_context threadPool;
  // Create the server endpoint
  Server(p, config.empty() ? "config.json" : config).start(threadPool);

  return 0;
}
