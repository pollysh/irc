#include "ServerCmd.hpp"
#include "Server.hpp"
#include <sstream>
#include <vector>

Server::Server() {
    registerCommandHandlers();
}

void Server::registerCommandHandlers() {
    commandHandlers["NICK"] = &Server::nickCmd;
    commandHandlers["USER"] = &Server::userCmd;
    commandHandlers["JOIN"] = &Server::joinCmd;
    commandHandlers["PRIVMSG"] = &Server::privMsgCmd;
    // Add more commands as needed
}

void Server::handleCommand(int clientFd, const std::string& commandLine) {
    // Parsing command and args...
    std::vector<std::string> args; // Assume this is filled with arguments
    std::string command; // Assume this is the extracted command

    CommandHandler handler = commandHandlers[command];
    if (handler != NULL) {
        (this->*handler)(clientFd, args);
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

void Server::userCmd(int clientFd, const std::vector<std::string>& args) {
    if (args.size() >= 1) {
        // Set the username for the client
        clients[clientFd].username = args[0];
        // Optionally, send confirmation or notification back to the client
    }
}
void Server::joinCmd(int clientFd, const std::vector<std::string>& args) {
    if (args.size() >= 1) {
        // Add the client to the specified channel
        channels[args[0]].push_back(clientFd);
        // Optionally, notify the channel or the client
    }
}

void Server::privMsgCmd(int clientFd, const std::vector<std::string>& args) {
    if (args.size() >= 2) {
        // args[0] is the target (username or channel), args[1] is the message
        string target = args[0];
        string message = args[1];
        // Implement logic to send a private message either to a user or broadcast in a channel
    }
}
