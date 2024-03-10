#include "Server.hpp"

int Server::getClientFdFromNickname(const std::string& targetNickname) {
    std::map<int, std::string>::const_iterator it;
    for (it = clientNicknames.begin(); it != clientNicknames.end(); ++it) {
        if (it->second == targetNickname) {
            return it->first;
        }
    }
    return -1;
}

void Server::nickCmd(int clientFd, const std::string& command) {
    // Assuming command is "NICK <nickname>\r\n"
    std::string nickname;
    std::size_t pos = command.find(" ");
    if (pos != std::string::npos) {
        nickname = command.substr(pos + 1);
        // Removing any trailing newlines or carriage returns
        nickname.erase(std::remove(nickname.begin(), nickname.end(), '\n'), nickname.end());
        nickname.erase(std::remove(nickname.begin(), nickname.end(), '\r'), nickname.end());
    }

    if (!nickname.empty()) {
        // Check for whitespace or more than one word in nickname.
        if (nickname.find(' ') != std::string::npos) {
            sendMessage(clientFd, "Error: Nickname '" + nickname + "' is invalid. It cannot contain spaces.");
            return;
        }

        // Check if the nickname already exists.
        bool nicknameExists = false;
        for (std::map<int, std::string>::iterator it = clientNicknames.begin(); it != clientNicknames.end(); ++it) {
            if (it->second == nickname) {
                nicknameExists = true;
                break;
            }
        }

        if (nicknameExists) {
            sendMessage(clientFd, "Error: Nickname '" + nickname + "' is already in use.");
        } else {
            clientNicknames[clientFd] = nickname;
            std::cout << "Client " << clientFd << " sets nickname to " << nickname << std::endl;
            sendMessage(clientFd, "NICK :" + nickname); // Echo the nickname change back to the client.
        }
    } else {
        sendMessage(clientFd, "Error: Nickname cannot be empty.");
    }
}

void Server::userCmd(int clientFd, const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    getline(iss >> std::ws, cmd, ' ');

    std::string username;
    getline(iss >> std::ws, username);

    if (!username.empty()) {
        bool usernameExists = false;
        for (std::map<int, std::string>::iterator it = clientUsernames.begin(); it != clientUsernames.end(); ++it) {
            if (it->second == username) {
                usernameExists = true;
                break;
            }
        }

        if (usernameExists) {
            sendMessage(clientFd, "Error: Username '" + username + "' is already in use.");
        } else {
            clientUsernames[clientFd] = username;
            std::cout << "Client " << clientFd << " sets username to " << username << std::endl;
            sendMessage(clientFd, "Username set to " + username);
        }
    } else if (username.empty()) {
        sendMessage(clientFd, "Error: Username cannot be empty.");
    } else {
        sendMessage(clientFd, "Error: Username cannot contain spaces or be multiple words.");
    }
}

bool Server::isClientOperatorOfChannel(int clientFd, const std::string& channel) {
    if (channels.find(channel) == channels.end()) {
        return false;
    }

    std::map<std::string, int>::iterator it = channelOperators.find(channel);
    if (it != channelOperators.end() && it->second == clientFd) {
        return true;
    }

    return false;
}


void Server::modeCmd(int clientFd, const std::string& channel, const std::string& mode, bool set, const std::string& password, const std::string& targetNickname) {
    
    if (!isClientOperatorOfChannel(clientFd, channel)) {
            sendMessage(clientFd, "Error: You must be an operator to change this mode.");
            return;
    }

    if (mode == "i") {
        channelInviteOnly[channel] = set;
        std::string modeStatus = set ? "enabled" : "disabled";
        sendMessage(clientFd, "Invite-only mode for " + channel + " has been " + modeStatus + ".");
        
        broadcastMessage(channel, "The channel " + channel + " is now in invite-only mode: " + modeStatus);
    } else if (mode == "t") {
        channelOperatorRestrictions[channel] = set;
        std::string status = set ? "enabled" : "disabled";
        sendMessage(clientFd, "Operator restrictions for " + channel + " are now " + status + ".");
        broadcastMessage(channel, "The channel " + channel + " now has operator restrictions " + status + ".");
    } else if (mode == "k") {
        if (set && !password.empty()) {
            channelPasswords[channel] = password;
            sendMessage(clientFd, "Password for " + channel + " has been set.");
            broadcastMessage(channel, "A password is now required to join " + channel + ".");
        } else if (!set) {
            channelPasswords.erase(channel);
            sendMessage(clientFd, "Password for " + channel + " has been removed.");
            broadcastMessage(channel, channel + " no longer requires a password to join.");
        } else {
            sendMessage(clientFd, "Error: A password is required to set the channel key.");
        }
    } else if (mode == "l") {
        if (set) {
            try {
                int limit = std::stoi(password);
                channelUserLimits[channel] = limit;
                sendMessage(clientFd, "User limit for " + channel + " has been set to " + std::to_string(limit) + ".");
            } catch (const std::invalid_argument& ia) {
                sendMessage(clientFd, "Error: Invalid user limit provided.");
            } catch (const std::out_of_range& oor) {
                sendMessage(clientFd, "Error: User limit is out of range.");
            }
        } else {
            channelUserLimits.erase(channel);
            sendMessage(clientFd, "User limit for " + channel + " has been removed.");
        }
    } else if (mode == "o"){
        if (channels.find(channel) == channels.end()) {
            sendMessage(clientFd, "Error: The specified channel does not exist.");
            return;
        }
        std::cout << "Nickname: " << targetNickname << std::endl;
        int targetFd = getClientFdFromNickname(targetNickname);
        if (targetFd == -1) {
            sendMessage(clientFd, "Error: User not found.");
            return;
        }

        if (!isClientOperatorOfChannel(clientFd, channel)) {
            sendMessage(clientFd, "Error: You are not an operator of the channel.");
            return;
        }

        if (set) {
            channelOperators[channel] = targetFd;
            sendMessage(clientFd, targetNickname + " is now an operator of " + channel + ".");
        } else {
            if (channelOperators[channel] == targetFd) { 
                channelOperators[channel] = -1;
                sendMessage(clientFd, targetNickname + " is no longer an operator of " + channel + ".");
            } else {
                sendMessage(clientFd, "Error: " + targetNickname + " is not an operator of the channel.");
            }
        }
    } else {
        sendMessage(clientFd, "Error: Unsupported mode.");
    }
}

void Server::topicCmd(int clientFd, const std::string& channel, const std::string& newTopic) {
    if (channels.find(channel) == channels.end()) {
        sendMessage(clientFd, "Error: The specified channel does not exist.");
        return;
    }

    bool restrictionsActive = channelOperatorRestrictions.count(channel) ? channelOperatorRestrictions[channel] : true;
    bool isOperator = (channelOperators.count(channel) && channelOperators[channel] == clientFd);

    if (!restrictionsActive || isOperator) {
        if (newTopic.empty()) {
            std::string currentTopic = channelTopics[channel];
            sendMessage(clientFd, currentTopic.empty() ? "No topic is set for " + channel + "." : 
                "Current topic for " + channel + ": " + currentTopic);
        } else {
            // Changing the topic
            channelTopics[channel] = newTopic;
            sendMessage(clientFd, "Topic for " + channel + " updated to: " + newTopic);
            if (!restrictionsActive) {
                broadcastMessage(channel, clientNicknames[clientFd] + " has changed the topic to: " + newTopic);
            }
        }
    } else {
        sendMessage(clientFd, "Error: Operator restrictions are in place. Only the operator can view or change the topic.");
    }
}

void Server::joinChannel(int clientFd, const std::string& channelName, const std::string& password) {

    bool isInviteOnly = channelInviteOnly.find(channelName) != channelInviteOnly.end() && channelInviteOnly[channelName];
    bool isInvited = channelInvitations[channelName].find(clientFd) != channelInvitations[channelName].end();

    if (isInviteOnly && !isInvited) {
        sendMessage(clientFd, "Error: " + channelName + " is invite-only, and you're not invited.");
        return;
    }

    if (clientNicknames.find(clientFd) == clientNicknames.end() || clientNicknames[clientFd].empty()) {
        std::cerr << "Client " << clientFd << " attempted to join a channel without setting a nickname." << std::endl;
        sendMessage(clientFd, "You must set a nickname before joining a channel.");
        return;
    }

    if (channelPasswords.find(channelName) != channelPasswords.end()) {
        if (channelPasswords[channelName] != password) {
            sendMessage(clientFd, "Error: Incorrect password for " + channelName + ". Please provide the correct password.");
            return;
        }
    }

    if (channels.find(channelName) != channels.end()) {
        std::vector<int>& clients = channels[channelName];
        
        if (channelUserLimits.find(channelName) != channelUserLimits.end() && clients.size() >= channelUserLimits[channelName]) {
            sendMessage(clientFd, "Error: User limit for " + channelName + " has been reached.");
            return;
        }
    }

    if (channelName.empty() || channelName[0] != '#') {
        std::cerr << "Client " << clientFd << " attempted to join an invalid channel name: " << channelName << std::endl;
        sendMessage(clientFd, "Invalid channel name. Channel names must start with a '#'.");
        return;
    }
    
    if (channels.find(channelName) == channels.end()) {
        channels[channelName] = std::vector<int>();
    } else {
        std::vector<int>& clients = channels[channelName];
        if (std::find(clients.begin(), clients.end(), clientFd) != clients.end()) {
            sendMessage(clientFd, "You are already a member of " + channelName);
            return;
        }
    }

    channels[channelName].push_back(clientFd);
    clientLastChannel[clientFd] = channelName; 
    std::cout << "Client " << clientFd << " with nickname " << clientNicknames[clientFd] << " joined channel " << channelName << std::endl;
    sendMessage(clientFd, "Welcome to " + channelName + "!");

    
    if (channels[channelName].size() == 1) {
        channelOperators[channelName] = clientFd;
        sendMessage(clientFd, "You are the operator of " + channelName + ".");
    }

    for (size_t i = 0; i < channels[channelName].size(); ++i) {
        int memberFd = channels[channelName][i];
        if (memberFd != clientFd) {
            sendMessage(memberFd, clientNicknames[clientFd] + " has joined the channel.");
        }
    }
}

void Server::inviteCmd(int clientFd, const std::string& channel, const std::string& targetNickname) {
    
    channelInvitations[channel].insert(getClientFdFromNickname(targetNickname)); 
    
    if (channels.find(channel) == channels.end()) {
        sendMessage(clientFd, "Error: The specified channel does not exist.");
        return;
    }

    if (channelOperators.find(channel) != channelOperators.end() && channelOperators[channel] == clientFd) {
        int targetFd = -1;
        for (std::map<int, std::string>::iterator it = clientNicknames.begin(); it != clientNicknames.end(); ++it) {
            if (it->second == targetNickname) {
                targetFd = it->first;
                break;
            }
        }
        
        if (targetFd == -1) {
            sendMessage(clientFd, "Error: User not found.");
            return;
        }

        // Before checking if the user is already in the channel, record the invitation
        channelInvitations[channel].insert(targetFd);

        std::vector<int>& clients = channels[channel];
        if (std::find(clients.begin(), clients.end(), targetFd) != clients.end()) {
            sendMessage(clientFd, "Error: User already in the channel.");
            return;
        }

        sendMessage(targetFd, "You have been invited to join " + channel + ". Use JOIN command to enter.");
        sendMessage(clientFd, "Invitation sent to " + targetNickname + ".");
    } else {
        sendMessage(clientFd, "Error: Only the channel operator can invite users.");
    }
}

void Server::kickCmd(int clientFd, const std::string& channel, const std::string& targetNickname) {
    if (channelOperators.find(channel) != channelOperators.end() && channelOperators[channel] == clientFd) {
        int targetFd = -1;
        for (std::map<int, std::string>::iterator it = clientNicknames.begin(); it != clientNicknames.end(); ++it) {
            if (it->second == targetNickname) {
                targetFd = it->first;
                break;
            }
        }
        
        if (targetFd == -1) {
            sendMessage(clientFd, "Error: User not found.");
            return;
        }

        std::vector<int>& clients = channels[channel];
        if (std::find(clients.begin(), clients.end(), targetFd) != clients.end()) {
            clients.erase(std::remove(clients.begin(), clients.end(), targetFd), clients.end());
            sendMessage(targetFd, "You have been kicked from " + channel + ".");
            broadcastMessage(channel, targetNickname + " has been kicked from the channel.");
        } else {
            sendMessage(clientFd, "Error: User not in the specified channel.");
        }
    } else {
        sendMessage(clientFd, "Error: Only the channel operator can kick users.");
    }
}

std::string toLower(const std::string& str) {
    std::string lowerStr;
    for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
        lowerStr += std::tolower(*it, std::locale());
    }
    return lowerStr;
}

std::string trim(const std::string& str) {
    std::string::size_type first = str.find_first_not_of(' ');
    if (first == std::string::npos) return "";
    std::string::size_type last = str.find_last_not_of(' ');
    return str.substr(first, last - first + 1);
}


void Server::sendPrivateMessage(int senderFd, const std::string& recipientNickname, const std::string& message) {
    std::string lowerRecipientNickname = toLower(trim(recipientNickname));

    int recipientFd = -1;
    for (std::map<int, std::string>::iterator it = clientNicknames.begin(); it != clientNicknames.end(); ++it) {
        if (toLower(trim(it->second)) == lowerRecipientNickname) {
            recipientFd = it->first;
            break;
        }
    }

    if (recipientFd != -1) {
        std::string senderNickname = clientNicknames[senderFd];
        // Format the message according to IRC standards for PRIVMSG
        std::string formattedMessage = ":" + senderNickname + "!" + senderNickname + "@server PRIVMSG " + recipientNickname + " :" + message;
        
        // Send the formatted message to the recipient
        sendMessage(recipientFd, formattedMessage);
    } else {
        // Error handling for when the user is not found
        sendMessage(senderFd, "Error: User '" + recipientNickname + "' not found.");
    }
}
