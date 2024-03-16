#include "Server.hpp"
#include <csignal>

void handle_signal(int sig) {
	(void)sig;
    std::cout << "Signal received" << std::endl;
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
        return 1;
    }
    signal(SIGINT, handle_signal);
    signal(SIGQUIT, handle_signal);
    signal(SIGTERM, handle_signal);
    int port = std::atoi(argv[1]);
    std::string password = argv[2];
    Server server(port, password);
    server.run();
    return 0;
}
