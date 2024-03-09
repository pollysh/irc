#include "Server.hpp"

void Server::processCommand(int clientFd, const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;

    if (cmd == "NICK") {
        std::string nickname;
        iss >> nickname;
        if (!nickname.empty()) {
            clientNicknames[clientFd] = nickname;
            std::cout << "Client " << clientFd << " sets nickname to " << nickname << std::endl;
            sendMessage(clientFd, "Nickname set to " + nickname);
        }
    } else if (cmd == "USER") {
        std::string username;
        iss >> username;
        if (!username.empty()) {
            clientUsernames[clientFd] = username;
            std::cout << "Client " << clientFd << " sets username to " << username << std::endl;
            sendMessage(clientFd, "Username set to " + username);
        }
    } else if (cmd == "JOIN") {
        std::string channel;
        std::string password = "";
        iss >> channel;

        if (!iss.eof()) {
            std::getline(iss, password);
            if (!password.empty() && password[0] == ' ') {
                password.erase(0, 1);
            }
        }
        joinChannel(clientFd, channel, password);
    } else if (cmd == "PRIVMSG") {
        std::string target;
        iss >> target;
    
        std::string message;
        std::getline(iss, message);
    
        if (!message.empty() && message[0] == ' ') {
            message.erase(0, 1);
        }

        if (!target.empty() && target[0] != '#') {
            sendPrivateMessage(clientFd, target, message);
        } else if (!target.empty()) {
            if (!message.empty()) {
                sendMessageToChannel(clientFd, target, message);
            }
        } else {
            sendMessage(clientFd, "Error: Invalid target for PRIVMSG.");
        }
    } else if (cmd == "KICK")
    {
        sendMessage(clientFd, "You are trying to kick");
        std::string channel, targetNickname;
        iss >> channel >> targetNickname;
        if (!channel.empty() && !targetNickname.empty()) {
            kickCmd(clientFd, channel, targetNickname);
        } else {
            sendMessage(clientFd, "Error: KICK command requires a channel and a user nickname.");
        }
    } else if (cmd == "INVITE")
    {
        std::string channel, targetNickname;
        iss >> channel >> targetNickname;
        if (!channel.empty() && !targetNickname.empty()) {
            inviteCmd(clientFd, channel, targetNickname);
        } else {
            sendMessage(clientFd, "Error: INVITE command requires a channel and a user nickname.");
        }
    } else if (cmd == "TOPIC")
    {
        std::string channel;
        std::string newTopic;
        iss >> channel;
        std::getline(iss, newTopic);
        if (!channel.empty()) {
            if (!newTopic.empty()) {
                newTopic = newTopic.substr(1);
            }
            topicCmd(clientFd, channel, newTopic);
        } else {
            sendMessage(clientFd, "Error: TOPIC command requires a channel name.");
        }
    } else if (cmd == "MODE") {
        std::string channel, modeArg, targetNickname;;
        iss >> channel >> modeArg;
        bool set = (modeArg[0] == '+');
        std::string mode = modeArg.substr(1, 1);
        std::string argument;

        if (mode == "k" || mode == "l" || mode == "o") {
        iss >> targetNickname;
        if (targetNickname.empty()) {
            sendMessage(clientFd, "Error: Missing argument for setting mode " + mode);
            return;
        } else {
            modeCmd(clientFd, channel, mode, set, targetNickname, targetNickname);
        }
        }
    }
}
