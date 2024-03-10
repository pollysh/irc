#include "Server.hpp"

void Server::broadcastMessage(const std::string& channelName, const std::string& message, int excludeFd) {
    std::vector<int>& clients = channels[channelName];
    for (size_t i = 0; i < clients.size(); ++i) {
        int memberFd = clients[i];
        if (memberFd != excludeFd) {
            sendMessage(memberFd, message);
        }
    }
}

void Server::sendMessage(int clientFd, const std::string& message) {
    std::string formattedMessage = message;
    if (!message.empty() && message[message.length() - 1] != '\n') {
        formattedMessage += "\n";
    }
    if (send(clientFd, formattedMessage.c_str(), formattedMessage.length(), 0) == -1) {
        std::cerr << "Failed to send message to client " << clientFd << ": " << strerror(errno) << std::endl;
    }
}

void Server::sendMessageToChannel(int clientFd, const std::string& channel, const std::string& message) {
    // Ensure the channel exists
    if (channels.find(channel) == channels.end()) {
        sendMessage(clientFd, "ERROR :No such channel.");
        return;
    }

    // Check if the client is part of the channel
    std::map<std::string, std::vector<int> >::iterator channelIt = channels.find(channel);
    bool isMember = false;
    for (std::vector<int>::iterator it = channelIt->second.begin(); it != channelIt->second.end(); ++it) {
        if (*it == clientFd) {
            isMember = true;
            break;
        }
    }

    if (!isMember) {
        sendMessage(clientFd, "ERROR :You're not a member of channel " + channel);
        return;
    }

    // Retrieve the sender's nickname or use a placeholder if not found
    std::string senderNickname = "Unknown";
    std::map<int, std::string>::iterator nickIt = clientNicknames.find(clientFd);
    if (nickIt != clientNicknames.end()) {
        senderNickname = nickIt->second;
    }

    // Format the message as per IRC standards
    std::string formattedMessage = ":" + senderNickname + "!user@host PRIVMSG " + channel + " :" + message;

    // Broadcast the message to all channel members except the sender
    for (std::vector<int>::iterator it = channelIt->second.begin(); it != channelIt->second.end(); ++it) {
        if (*it != clientFd) {
            sendMessage(*it, formattedMessage);
        }
    }
}

void Server::processClientMessage(int clientFd, const std::string& rawMessage) {
    // Trim the message and split by spaces to identify the command and its parameters
    std::string trimmedMessage = trim(rawMessage);
    std::istringstream iss(trimmedMessage);
    std::string command;
    iss >> command;

    // Handle PASS command for authentication
    if (command == "PASS") {
        std::string providedPassword;
        iss >> providedPassword;
        if (providedPassword == serverPassword) {
            clientAuthenticated[clientFd] = true;
            sendMessage(clientFd, ":Server 001 :Password accepted. You are now authenticated.");
        } else {
            sendMessage(clientFd, ":Server 464 :Incorrect password. Please try again.");
        }
        return;
    }

    // Require authentication for any further commands
    if (!clientAuthenticated[clientFd]) {
        sendMessage(clientFd, ":Server 464 :Please authenticate with the PASS command.");
        return;
    }

    // Process recognized commands
    if (command == "JOIN" || command == "PRIVMSG" || command == "NICK" || command == "USER" ||
        command == "KICK" || command == "INVITE" || command == "TOPIC" || command == "MODE") {
        processCommand(clientFd, trimmedMessage);
    } else if (!command.empty()) {
        // Handle unrecognized commands or messages outside channels
        sendMessage(clientFd, ":Server 421 " + command + " :Unknown command");
    } else {
        // Handle case where command is not recognized and it's not a channel message
        sendMessage(clientFd, ":Server 411 :You haven't joined any channel or missing command.");
    }
}
