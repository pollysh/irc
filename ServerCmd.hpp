// Server.h
#ifndef SERVERCMD_HPP
#define SERVERCMD_HPP

#include "ClientInfo.hpp"
#include <map>
#include <functional>

class Server {
private:
    std::map<int, ClientInfo> clients;
    std::map<std::string, std::function<void(int, const std::vector<std::string>&)>> commandHandlers;

public:
    Server();
    void registerCommandHandlers();
    void handleCommand(int clientFd, const std::string& command);
    void nickCmd(int clientFd, const std::vector<std::string>& args);
    // Other command handlers...
};

#endif // SERVER_H
