#include "Server.hpp"

void Server::processCommand(int clientFd, const std::string& command) {
    std::cout << "process command" << std::endl;
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
        std::string password = ""; // Initialize password as empty
        iss >> channel;

    // Check if there's more input (potential password)
        if (!iss.eof()) {
            std::getline(iss, password);
        // Remove the leading space
            if (!password.empty() && password[0] == ' ') {
                password.erase(0, 1);
            }
        }
        joinChannel(clientFd, channel, password);
    } else if (cmd == "PRIVMSG") {
        std::string target;
        iss >> target; // Use >> to properly extract the target, trimming leading spaces
    
        std::string message;
        std::getline(iss, message); // Capture the rest of the input as the message
    
        if (!message.empty() && message[0] == ' ') {
        // Remove the leading space before the message content
            message.erase(0, 1);
        }

        if (!target.empty() && target[0] != '#') {
        // This is a private message to a user
            sendPrivateMessage(clientFd, target, message);
        } else if (!target.empty()) {
        // This is a message to a channel
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
        iss >> channel >> targetNickname; // Extract channel name and nickname from the command
        if (!channel.empty() && !targetNickname.empty()) {
            kickCmd(clientFd, channel, targetNickname); // Call kickCmd with extracted arguments
        } else {
            sendMessage(clientFd, "Error: KICK command requires a channel and a user nickname.");
        }
    } else if (cmd == "INVITE")
    {
        std::string channel, targetNickname;
        iss >> channel >> targetNickname; // Extract channel name and nickname from the command
        if (!channel.empty() && !targetNickname.empty()) {
            inviteCmd(clientFd, channel, targetNickname); // Call inviteCmd with extracted arguments
        } else {
            sendMessage(clientFd, "Error: INVITE command requires a channel and a user nickname.");
        }
    } else if (cmd == "TOPIC")
    {
        std::string channel;
        std::string newTopic;
        iss >> channel;
        std::getline(iss, newTopic); // Use getline to allow spaces in the topic
        if (!channel.empty()) {
            if (!newTopic.empty()) {
                newTopic = newTopic.substr(1); // Remove the leading space
            }
            topicCmd(clientFd, channel, newTopic);
        } else {
            sendMessage(clientFd, "Error: TOPIC command requires a channel name.");
        }
    } else if (cmd == "MODE") {
        std::string channel, modeArg, targetNickname;;
        iss >> channel >> modeArg;
    // Checking if setting (+) or removing (-) a mode
        bool set = (modeArg[0] == '+');
        std::string mode = modeArg.substr(1, 1); // Extract the mode character (e.g., 'k' or 'l')
        std::string argument; // Could be a password or user limit

        if (mode == "k" || mode == "l" || mode == "o") {
        // Extract the argument or targetNickname
        iss >> targetNickname;
        if (targetNickname.empty()) {
            sendMessage(clientFd, "Error: Missing argument for setting mode " + mode);
            return;
        } else {
        // Handling other modes without additional data
            modeCmd(clientFd, channel, mode, set, targetNickname, targetNickname);
        }
        }
    }
}
