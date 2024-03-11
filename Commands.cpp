#include "Server.hpp"

void Server::showMessage(const std::string& channelName, const std::string& message) {
    if (channels.find(channelName) != channels.end()) {
        std::vector<int>& clients = channels[channelName];
        for (size_t i = 0; i < clients.size(); ++i) {
            sendMessage(clients[i], message);
        }
    }
}

int Server::getClientFdFromNickname(const std::string& targetNickname) {
    std::map<int, std::string>::const_iterator it;
    for (it = clientNicknames.begin(); it != clientNicknames.end(); ++it) {
        if (it->second == targetNickname) {
            return it->first;
        }
    }
    return -1;
}

bool Server::nickCmd(int clientFd, const std::string& command) {
   
    std::string nickname = command;
    std::cout <<"Nickname: " << command << std::endl;

    if (!nickname.empty()) {
        if (nickname.find(' ') != std::string::npos) {
            sendMessage(clientFd, "Error: Nickname '" + nickname + "' is invalid. It cannot contain spaces.");
        }

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
            sendMessage(clientFd, "NICK :" + nickname);
            return true;
        }
    } else 
        sendMessage(clientFd, "Error: Nickname cannot be empty.");
    return false;
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
        
        showMessage(channel, "The channel " + channel + " is now in invite-only mode: " + modeStatus);
    } else if (mode == "t") {
        channelOperatorRestrictions[channel] = set;
        std::string status = set ? "enabled" : "disabled";
        sendMessage(clientFd, "Operator restrictions for " + channel + " are now " + status + ".");
        showMessage(channel, "The channel " + channel + " now has operator restrictions " + status + ".");
    } else if (mode == "k") {
        if (set && !password.empty()) {
            channelPasswords[channel] = password;
            sendMessage(clientFd, "Password for " + channel + " has been set.");
            showMessage(channel, "A password is now required to join " + channel + ".");
        } else if (!set) {
            channelPasswords.erase(channel);
            sendMessage(clientFd, "Password for " + channel + " has been removed.");
            showMessage(channel, channel + " no longer requires a password to join.");
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

void Server::sendNumericReply(int clientFd, int numericCode, const std::string& message) {
    std::ostringstream ss;
    std::string clientNick = clientNicknames[clientFd].empty() ? "*" : clientNicknames[clientFd];

    // Format: ":ServerName NumericCode ClientNick :Message"
    // ServerName can be a placeholder if you don't have a specific name for your server.
    // Numeric codes should be padded to three digits.
    ss << ":Server " << std::setw(3) << std::setfill('0') << numericCode << " " << clientNick << " :" << message << "\r\n";

    std::string formattedMessage = ss.str();
    sendMessage(clientFd, formattedMessage);
}

void Server::topicCmd(int clientFd, const std::string& channel, const std::string& newTopic) {
    if (channels.find(channel) == channels.end()) {
        sendNumericReply(clientFd, ERR_NOSUCHCHANNEL, channel + " :No such channel");
        return;
    }

    bool restrictionsActive = channelOperatorRestrictions.count(channel) ? channelOperatorRestrictions[channel] : true;
    bool isOperator = channelOperators.count(channel) && channelOperators[channel] == clientFd;

    if (!restrictionsActive || isOperator) {
        if (newTopic.empty()) {
            std::string currentTopic = channelTopics[channel];
            if (currentTopic.empty()) {
                sendNumericReply(clientFd, RPL_NOTOPIC, channel + " :No topic is set");
            } else {
                sendNumericReply(clientFd, RPL_TOPIC, channel + " :" + currentTopic);
            }
        } else {
            channelTopics[channel] = newTopic;
            // Broadcast the topic change to all channel members
            std::string topicChangeMsg = ":" + clientNicknames[clientFd] + "!user@host TOPIC " + channel + " :" + newTopic;
            broadcastMessage(channel, topicChangeMsg, -1); // Assuming broadcastMessage skips sending to excludeFd if it's -1
        }
    } else {
        sendNumericReply(clientFd, ERR_CHANOPRIVSNEEDED, channel + " :You're not channel operator");
    }
}

void Server::joinChannel(int clientFd, const std::string &channelName, const std::string &password) {
    (void)password;
    if (channelName.empty() || channelName[0] != '#') {
        sendNumericReply(clientFd, 403, "No such channel");
        return;
    }

    if (clientNicknames.find(clientFd) == clientNicknames.end() || clientNicknames[clientFd].empty()) {
        sendNumericReply(clientFd, 431, "No nickname given");
        return;
    }

    bool isNewChannel = channels.find(channelName) == channels.end();
    if (!isNewChannel && std::find(channels[channelName].begin(), channels[channelName].end(), clientFd) != channels[channelName].end()) {
        return; 
    }

    if (isNewChannel) {
        channels[channelName] = std::vector<int>();
        channelOperators[channelName] = clientFd; 
    }

    channels[channelName].push_back(clientFd);
    clientLastChannel[clientFd] = channelName;
    std::string nick = clientNicknames[clientFd];
    std::string user = "user"; 
    std::string host = "localhost"; 
    std::string joinMsg = ":" + nick + "!" + user + "@" + host + " JOIN :" + channelName;

    sendMessage(clientFd, joinMsg);
    broadcastMessage(channelName, joinMsg, clientFd);

    // Send names list to the joining client
    std::string namesList = "353 " + nick + " = " + channelName + " :";
    for (size_t i = 0; i < channels[channelName].size(); ++i) {
        namesList += clientNicknames[channels[channelName][i]] + " ";
    }
    namesList.erase(namesList.end() - 1); // Remove the last space
    std::string nameReply = "= " + channelName + " :" + namesList;
    sendNumericReply(clientFd, RPL_NAMREPLY, nameReply);
    sendNumericReply(clientFd, RPL_ENDOFNAMES, channelName + " :End of /NAMES list.");


    // If it's a new channel, send mode +o for the joining user
    if (isNewChannel) {
        std::string modeChangeMsg = ":" + nick + "!user@host MODE " + channelName + " +o " + nick + "\r\n"; // IRC message ends with CR LF
        broadcastMessage(channelName, modeChangeMsg, -1); // Corrected part, assuming broadcastMessage function handles -1 as "send to all"
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
            broadcastMessage(channel, targetNickname + " has been kicked from the channel.", targetFd);
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

std::string Server::trim(const std::string& str) {
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
