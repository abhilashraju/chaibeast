
#include "command_line_parser.hpp"
#include "request_forwarder.hpp"
#include "server.hpp"
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
  if (config.empty()) {
    config = "config.json";
  }
  RequestForwarder router({config.data(), config.length()});
  // SocketStreamMaker streamMaker;
  SSlServer(p, router,
            "/Users/abhilashraju/work/cpp/chai/certs/server-certificate.pem",
            "/Users/abhilashraju/work/cpp/chai/certs/server-private-key.pem",
            "/etc/ssl/certs/authority")
      .start(threadPool);

  return 0;
}
