#include "ServerCmd.hpp"
#include <sstream>
#include <vector>

Server::Server() {
    registerCommandHandlers();
}


void Server::registerCommandHandlers() {
    // Register command handlers
    commandHandlers["NICK"] = std::bind(&Server::nickCmd, this, std::placeholders::_1, std::placeholders::_2);
    // Register other command handlers...
}

void Server::handleCommand(int clientFd, const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    std::vector<std::string> args;
    iss >> cmd; // Extract the command
    std::string arg;
    while (iss >> arg) args.push_back(arg); // Extract arguments

    // Find and execute the command handler if it exists
    auto handler = commandHandlers.find(cmd);
    if (handler != commandHandlers.end()) {
        handler->second(clientFd, args);
    } else {
        // Handle unknown command
    }
}

void Server::nickCmd(int clientFd, const std::vector<std::string>& args) {
    if (args.size() > 0) {
        // Set the client's nickname
        clients[clientFd].nickname = args[0];
        // Acknowledge the nickname change to the client, etc.
    }
}
