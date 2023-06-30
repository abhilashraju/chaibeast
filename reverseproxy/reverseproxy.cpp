
#include "command_line_parser.hpp"
#include "proxyserver.hpp"
using namespace chai;
int main(int argc, const char* argv[])
{
    auto [p, d, dp, config] = getArgs(parseCommandline(argc, argv), "-p", "-d",
                                      "-dp", "-c");
    if (p.empty() || d.empty() || dp.empty())
    {
        std::cout << "Invalid Invokation. Argument Missing\n";
        std::cout
            << "Please use\n reverseproxy -p port -d default_forwarding_ip -dp default_forwarding_port\n";
        return 0;
    }
    exec::single_thread_context threadPool;
    // Create the server endpoint
    Server(p, d, dp, config.empty() ? "config.json" : config).start(threadPool);

    return 0;
}
