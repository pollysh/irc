#include "Server.hpp"

void Server::broadcastMessage(const std::string& channelName, const std::string& message, int excludeFd) {
    if (channels.find(channelName) == channels.end()) {
        std::cerr << "Channel not found: " << channelName << std::endl;
        return;
    }

    std::vector<int>& clients = channels[channelName];
    std::vector<int>::iterator it = clients.begin();
    
    while (it != clients.end()) {
        int memberFd = *it;
        if (memberFd != excludeFd) {
            std::string formattedMessage = message + "\r\n"; // Ensure message ends with \r\n
            if (send(memberFd, formattedMessage.c_str(), formattedMessage.length(), 0) == -1) {
                std::cerr << "Failed to send message to client " << memberFd << ": " << strerror(errno) << std::endl;
                // Erase and close inside the condition to avoid skipping elements after erase
                close(memberFd); // Ensure the socket is closed
                clientAuthenticated.erase(memberFd);
                clientNicknames.erase(memberFd);
                it = clients.erase(it); // Adjust iterator post-erase
            } else {
                ++it; // Only increment if no erase was performed
            }
        } else {
            ++it; // Increment if this client is excluded from broadcast
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

void Server::redirectMessageToOtherChannel(int clientFd, const std::string& message) {
    bool messageRedirected = false;
    for (std::map<std::string, std::vector<int> >::iterator it = channels.begin(); it != channels.end(); ++it) {
        std::vector<int>& members = it->second;
        if (std::find(members.begin(), members.end(), clientFd) != members.end()) {
            sendMessageToChannel(clientFd, it->first, message);
            messageRedirected = true;
            break;
        }
    }

    if (!messageRedirected) {
        sendMessage(clientFd, "ERROR: You're not a member of any channel or hasn't identified with PASS.\r\n");
    }
}

std::string Server::formatMessageForChannel(int clientFd, const std::string& channel, const std::string& message) {
    std::string senderNickname = "Unknown";
    if(clientNicknames.find(clientFd) != clientNicknames.end()) {
        senderNickname = clientNicknames[clientFd]; // Found the nickname
    }

    std::string formattedMessage = ":" + senderNickname + "!user@host PRIVMSG " + channel + " :" + message;

    return formattedMessage;
}


void Server::sendMessageToChannel(int clientFd, const std::string& channel, const std::string& message) {
    if (channels.find(channel) == channels.end()) {
        sendMessage(clientFd, "ERROR :No such channel.\r\n");
        return;
    }

    std::vector<int>& members = channels[channel];
    if (std::find(members.begin(), members.end(), clientFd) == members.end()) {
        redirectMessageToOtherChannel(clientFd, message);
        return;
    }

    std::string formattedMessage = formatMessageForChannel(clientFd, channel, message);
    for (std::vector<int>::iterator it = members.begin(); it != members.end(); ++it) {
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
            auth++;
        }
        else
            sendMessage(clientFd, ":Server 464 :Incorrect password. Please try again.");
    }
    else if (command == "NICK")
    {
        if (nickCmd(clientFd, argm))
            auth++;
    }
    else if (command == "USER")
    {
        userCmd(clientFd, argm);
        auth++;
    }
    else if (auth == 3)
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
    std::string trimmedMessage = trim(rawMessage);
    std::istringstream iss(trimmedMessage);
    std::string command;
    
    if (!clientAuthenticated[clientFd])
    {
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
        sendToLastJoinedChannel(clientFd, trimmedMessage);
    }
}
