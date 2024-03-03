#include "ServerCmd.hpp"
#include "Server.hpp"
#include <sstream>
#include <vector>
#include <sys/socket.h>

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

void Server::sendMessage(int clientFd, const std::string& message) {
    // Simple wrapper around send to include error handling for C++98
    if (send(clientFd, message.c_str(), message.length(), 0) == -1) {
        // Handle send error, such as logging or queuing for retry
    }
}

void Server::privMsgCmd(int clientFd, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        // Not enough arguments provided for PRIVMSG
        return;
    }

    const std::string& target = args[0];
    // Concatenate all elements after args[0] to form the complete message
    std::string message;
    for (size_t i = 1; i < args.size(); ++i) {
        message += args[i];
        if (i < args.size() - 1) {
            message += " "; // Add space between words, but not after the last word
        }
    }

    if (target[0] == '#') { // Target is a channel
        if (channels.find(target) != channels.end()) {
            for (std::vector<int>::const_iterator it = channels[target].begin(); it != channels[target].end(); ++it) {
                int memberFd = *it;
                if (memberFd != clientFd) { // Skip sender
                    // Assuming you have a function to wrap the send call and handle errors
                    sendMessage(memberFd, message);
                }
            }
        } else {
            // Optionally, inform the sender that the channel does not exist
        }
    } else { // Target is assumed to be a username
        bool userFound = false;
        for (std::map<int, std::string>::const_iterator it = clientUsernames.begin(); it != clientUsernames.end(); ++it) {
            if (it->second == target) {
                userFound = true;
                sendMessage(it->first, message); // Send message
                break;
            }
        }
        if (!userFound) {
            // Optionally, inform the sender that the user does not exist
        }
    }
}


