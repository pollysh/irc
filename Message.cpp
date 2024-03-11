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
    // Ensure the message ends with \r\n (IRC protocol line endings)
    if (!message.empty() && (message.length() < 2 || message.substr(message.length() - 2) != "\r\n")) {
        formattedMessage += "\r\n";
    }
    if (send(clientFd, formattedMessage.c_str(), formattedMessage.length(), 0) == -1) {
        std::cerr << "Failed to send message to client " << clientFd << ": " << strerror(errno) << std::endl;
    }
}

void Server::sendMessageToChannel(int clientFd, const std::string& channel, const std::string& message) {
    if (channels.find(channel) == channels.end()) {
        sendMessage(clientFd, "ERROR :No such channel.");
        return;
    }

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

    std::string senderNickname = "Unknown";
    std::map<int, std::string>::iterator nickIt = clientNicknames.find(clientFd);
    if (nickIt != clientNicknames.end()) {
        senderNickname = nickIt->second;
    }

    std::string formattedMessage = ":" + senderNickname + "!user@host " + channel + " :" + message;

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

    // Check if the client is not authenticated yet
    if (!clientAuthenticated[clientFd]) {
        if (command == "PASS") {
            std::string providedPassword;
            iss >> providedPassword;
            if (providedPassword == serverPassword) {
                clientAuthenticated[clientFd] = true;
                sendMessage(clientFd, ":Server 001 :Password accepted. You are now authenticated.");
                // Optionally, prompt the client for the next steps
                sendMessage(clientFd, ":Server 002 :Please proceed with NICK and USER commands.");
            } else {
                sendMessage(clientFd, ":Server 464 :Incorrect password. Please try again.");
            }
        } else {
            // If the client attempts any command other than PASS without being authenticated
            sendMessage(clientFd, ":Server 464 :Authentication required. Please authenticate with the PASS command.");
        }
        return; // Stop further processing until authenticated
    }

    // If authenticated, process other commands
    if (command == "JOIN" || command == "PRIVMSG" || command == "NICK" || command == "USER" ||
        command == "KICK" || command == "INVITE" || command == "TOPIC" || command == "MODE") {
        processCommand(clientFd, trimmedMessage);
    } else {
            std::map<int, std::string>::iterator it = clientLastChannel.find(clientFd);
        if (it != clientLastChannel.end() && !it->second.empty()) {
            // If there's a last joined channel, forward the message there
            sendMessageToChannel(clientFd, it->second, trimmedMessage);
        } else {
            // Inform the client that they need to join a channel first
            sendMessage(clientFd, ":Server 411 :You haven't joined any channel or missing command.");
        }
    }
}
