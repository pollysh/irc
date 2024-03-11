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

std::string Server::formatMessage(const std::string &senderNickname, const std::string &message)
{
    return (" " + senderNickname + " :" + message);
}

void Server::sendMessage(int clientFd, const std::string &message)
{
    std::string formattedMessage = message;

    std::cout << "Sending Message: " << formattedMessage << std::endl; // Ensure the message ends with \r\n (IRC protocol line endings)
    if (!message.empty() && (message.length() < 2 || message.substr(message.length() - 2) != "\r\n"))
    {
        formattedMessage += "\r\n";
    }
    if (send(clientFd, formattedMessage.c_str(), formattedMessage.length(), 0) == -1)
    {
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

bool Server::processInitialCommand(int clientFd, const std::string &command, std::istringstream &iss)
{
    static int auth = 0;
    std::string argm;
    iss >> argm;
    
    if (command == "PASS")
    {
        
        if (argm == serverPassword)
        {
            clientAuthenticated[clientFd] = true;
            ++auth;
            sendMessage(clientFd, "Password accepted. You are now authenticated.");
        }
        else
            sendMessage(clientFd, ":Server 464 :Incorrect password. Please try again.");
    }
    if (command == "NICK")
        if (nickCmd(clientFd, argm))
            auth++;

    if (command == "USER")
    {
        userCmd(clientFd, argm);
        auth++;
    }
    if (auth == 3)
    {
        std::string nick = clientNicknames[clientFd];
        clientAuthenticated[clientFd] = true;
        sendMessage(clientFd, ":Server 001" + formatMessage(nick, "Welcome. You are now authenticated."));
        sendMessage(clientFd, ":Server 003" + formatMessage(nick, "For a list of commands, type /help."));
        sendMessage(clientFd, ":Server 005" + formatMessage(nick, "CHANTYPES=#"));
        sendMessage(clientFd, ":Server 005" + formatMessage(nick, "CHANMODES=i,t,k,o,l"));
        return true;
    }
    return false;
}

void Server::processClientMessage(int clientFd, const std::string &rawMessage)
{
    // Trim the message and split by spaces to identify the command and its parameters
    std::string trimmedMessage = trim(rawMessage);
    std::istringstream iss(trimmedMessage);
    std::string command;

    if (!clientAuthenticated[clientFd])
    {
         sendMessage(clientFd, "hello");
        while (iss >> command)
        {
            if (command == "PASS" || command == "NICK" || command == "USER")
                processInitialCommand(clientFd, command, iss);
        }
        if (clientAuthenticated[clientFd])
            return;
    }

    iss >> command;
    // If authenticated, process other commands
    if (command == "JOIN" || command == "PRIVMSG" || command == "NICK" || command == "USER" ||
        command == "KICK" || command == "INVITE" || command == "TOPIC" || command == "MODE")
    {
        processCommand(clientFd, trimmedMessage);
    }
    else
    {
        std::map<int, std::string>::iterator it = clientLastChannel.find(clientFd);
        if (it != clientLastChannel.end() && !it->second.empty())
        {
            // If there's a last joined channel, forward the message there
            sendMessageToChannel(clientFd, it->second, trimmedMessage);
        }
        else
        {
             // Inform the client that they need to join a channel first
             sendMessage(clientFd, ":Server 411 :You haven't joined any channel or missing command.");
         }
    }
}
