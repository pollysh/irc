#include "Server.hpp"

void Server::broadcastMessage(const std::string& channelName, const std::string& message) {
    if (channels.find(channelName) != channels.end()) {
        std::vector<int>& clients = channels[channelName];
        for (size_t i = 0; i < clients.size(); ++i) {
            sendMessage(clients[i], message);
        }
    }
}

void Server::sendMessage(int clientFd, const std::string& message) {
    std::string formattedMessage = message;
    if (!message.empty() && message[message.length() - 1] != '\n') {
        formattedMessage += "\n"; // Append a newline character if not present
    }
    if (send(clientFd, formattedMessage.c_str(), formattedMessage.length(), 0) == -1) {
        std::cerr << "Failed to send message to client " << clientFd << ": " << strerror(errno) << std::endl;
    }
}

void Server::sendMessageToChannel(int clientFd, const std::string& channel, const std::string& message) {
    if (channels.find(channel) == channels.end()) {
        sendMessage(clientFd, "Channel does not exist or you haven't joined any channel.");
        return;
    }

    std::map<int, std::string>::iterator itNick = clientNicknames.find(clientFd);
    std::string senderNickname;
    if (itNick != clientNicknames.end()) {
        senderNickname = itNick->second;
    } else {
        senderNickname = "Unknown";
    }

    // Construct the message to include the sender's nickname
    std::string formattedMessage = senderNickname + ": " + message;

    // Retrieve the list of channel members and send the message
    std::vector<int>& members = channels[channel];
    for (std::vector<int>::iterator it = members.begin(); it != members.end(); ++it) {
        int memberId = *it;
        if (memberId != clientFd) { // Don't send the message back to the sender
            sendMessage(memberId, formattedMessage);
        }
    }
}


void Server::processClientMessage(int clientFd, const std::string& message) {
    std::istringstream iss(message);
    std::string firstWord;
    iss >> firstWord;

    // Handle the PASS command for authentication
    if (firstWord == "PASS") {
        std::string providedPassword;
        iss >> providedPassword;
        if (providedPassword == serverPassword) {
            clientAuthenticated[clientFd] = true;
            sendMessage(clientFd, "Password accepted. You are now authenticated.");
        } else {
            sendMessage(clientFd, "Incorrect password. Please try again.");
        }
        return; // Early return to stop processing
    }

    // Check if the client is authenticated before processing any other command
    if (!clientAuthenticated[clientFd]) {
        sendMessage(clientFd, "Please authenticate with the PASS command.");
        return; // Block further command processing until authenticated
    }

    // Proceed with other commands only if the client is authenticated
    if (clientAuthenticated[clientFd]) {
        if (firstWord == "JOIN" || firstWord == "PRIVMSG" || firstWord == "NICK" || firstWord == "USER"
            || firstWord == "KICK" || firstWord == "INVITE" || firstWord == "TOPIC" || firstWord == "MODE") {
            processCommand(clientFd, message);
        } else {
            // Check if the client has joined any channel
            std::map<int, std::string>::iterator it = clientLastChannel.find(clientFd);
            if (it != clientLastChannel.end() && !it->second.empty()) {
                sendMessageToChannel(clientFd, it->second, message);
            } else {
                sendMessage(clientFd, "You haven't joined any channel.\n");
            }
        }
    }
}
