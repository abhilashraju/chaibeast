
#include "command_line_parser.hpp"
#include "request_forwarder.hpp"
#include "server.hpp"
using namespace chai;
int main(int argc, const char* argv[])
{
    auto [p, config] = getArgs(parseCommandline(argc, argv), "-p", "-c");
    if (p.empty())
    {
        std::cout << "Invalid Invocation. Argument Missing\n";
        std::cout << "Please use\n reverseproxy -p port -c config.json_path \n";
        return 0;
    }
    exec::static_thread_pool threadPool;
    // Create the server endpoint
    if (config.empty())
    {
        config = "config.json";
    }
    RequestForwarder router({config.data(), config.length()});

#ifdef SSL_ON
    SSlServer server(
        p, router,
        "/Users/abhilashraju/work/cpp/chai/certs/server-certificate.pem",
        "/Users/abhilashraju/work/cpp/chai/certs/server-private-key.pem",
        "/etc/ssl/certs/authority");
#else
    TCPServer server(p, router);
#endif
    server.start(threadPool);

    return 0;
}
