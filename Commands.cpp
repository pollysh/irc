#include "Server.hpp"

void Server::sendToLastJoinedChannel(int clientFd, const std::string& message) {
    if (clientLastChannel.find(clientFd) != clientLastChannel.end() && !clientLastChannel[clientFd].empty()) {
        std::string lastChannel = clientLastChannel[clientFd];
        if (channels.find(lastChannel) != channels.end()) {
            sendMessageToChannel(clientFd, lastChannel, message);
        } else {
            sendMessage(clientFd, "ERROR: Your last joined channel is no longer available.\r\n");
        }
    } else {
        sendMessage(clientFd, "ERROR: You haven't joined any channel.\r\n");
    }
}

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
       // if (nickname.find(' ') != std::string::npos) {
       //     sendMessage(clientFd, "Error: Nickname '" + nickname + "' is invalid. It cannot contain spaces.");
       // }

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
        if (set) {
            // Trim the password before saving it
            std::string trimmedPassword = trim(password);
            if (!trimmedPassword.empty()) {
                channelPasswords[channel] = trimmedPassword;
                sendMessage(clientFd, "Password for " + channel + " has been set.");
            } else {
                sendMessage(clientFd, "Error: A password is required to set mode +k.");
            }
        } else {
            channelPasswords.erase(channel);
            sendMessage(clientFd, "Password for " + channel + " has been removed.");
        }
    } else if (mode == "l") {
        if (set) {
            try {
                int limit = std::atoi(password.c_str());
                channelUserLimits[channel] = limit;
                std::ostringstream convert;
                convert << limit;
                std::string limitAsString = convert.str();
                sendMessage(clientFd, "User limit for " + channel + " has been set to " + limitAsString + ".");
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
            std::string topicChangeMsg = ":" + clientNicknames[clientFd] + "!user@host TOPIC " + channel + " :" + newTopic;
            broadcastMessage(channel, topicChangeMsg, -1); 
        }
    } else {
        sendNumericReply(clientFd, ERR_CHANOPRIVSNEEDED, channel + " :You're not channel operator");
    }
}

std::string visualizeWhitespace(const std::string& input) {
    std::ostringstream oss;
    for (std::string::const_iterator it = input.begin(); it != input.end(); ++it) {
        switch (*it) {
            case '\n': oss << "\\n"; break;
            case '\t': oss << "\\t"; break;
            case '\r': oss << "\\r"; break;
            case ' ': oss << "_"; break; // Visualize space with underscore for clarity
            default: oss << *it; // Regular character
        }
    }
    return oss.str();
}

std::string trimPassword(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n"); // Include \n for newline characters as well
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

void Server::joinChannel(int clientFd, const std::string &channelName, const std::string &password) {
    std::string visualizedPassword = visualizeWhitespace(password);
    std::cout << "Visualized password input: '" << visualizedPassword << "'" << std::endl;
    std::string trimmedPassword = trimPassword(password);

    if (channelName.empty() || channelName[0] != '#') {
        sendNumericReply(clientFd, 403, "No such channel");
        return;
    }

    if (channelInviteOnly[channelName] && channelInvitations[channelName].find(clientFd) == channelInvitations[channelName].end()) {
        sendNumericReply(clientFd, 473, channelName + " :Cannot join channel (+i) - invite only");
        return;
    }

    std::cout << "Attempting to join channel: " << channelName 
          << " | Exists: " << (channels.find(channelName) != channels.end() ? "Yes" : "No")
          << " | Requires password: " << (!channelPasswords[channelName].empty() ? "Yes" : "No")
          << " | Provided password: '" << trimmedPassword << "'"
          << " | Expected password: '" << channelPasswords[channelName] << "'"
          << " | Invite-only: " << (channelInviteOnly.find(channelName) != channelInviteOnly.end() && channelInviteOnly[channelName] ? "Yes" : "No") << std::endl;

    if (!channelPasswords[channelName].empty() && trimmedPassword != channelPasswords[channelName]) {
        sendNumericReply(clientFd, 475, channelName + " :Cannot join channel (+k) - wrong channel key");
        return;
    }

    if (channelUserLimits[channelName] > 0 && channels[channelName].size() >= static_cast<size_t>(channelUserLimits[channelName])) {
        sendNumericReply(clientFd, 471, channelName + " :Cannot join channel (+l) - channel is full");
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

    if (channelInviteOnly[channelName]) {
        sendNumericReply(clientFd, 475, channelName + " :Cannot join channel (+i) - invite required");
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

    std::string namesList = "353 " + nick + " = " + channelName + " :";
    for (size_t i = 0; i < channels[channelName].size(); ++i) {
        namesList += clientNicknames[channels[channelName][i]] + " ";
    }
    namesList.erase(namesList.end() - 1);
    std::string nameReply = "= " + channelName + " :" + namesList;
    sendNumericReply(clientFd, RPL_NAMREPLY, nameReply);
    sendNumericReply(clientFd, RPL_ENDOFNAMES, channelName + " :End of /NAMES list.");


    if (isNewChannel) {
        std::string modeChangeMsg = ":" + nick + "!user@host MODE " + channelName + " +o " + nick + "\r\n";
        broadcastMessage(channelName, modeChangeMsg, -1); 
    }
    clientLastChannel[clientFd] = channelName;
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
    std::string::size_type last = str.find_last_not_of(' ');

    if (first == std::string::npos || last == std::string::npos) return "";

    return str.substr(first, last - first + 1);
}

void Server::sendPrivateMessage(int senderFd, const std::string& recipientNickname, const std::string& message) {
    std::string senderNickname = clientNicknames[senderFd];

    if (!recipientNickname.empty() && recipientNickname[0] == '#') {
        std::string channelName = recipientNickname;
        if (channels.find(channelName) != channels.end()) {
            std::string formattedMessage = ":" + senderNickname + "!" + senderNickname + "@server PRIVMSG " + channelName + " :" + message;

         
            std::vector<int>& members = channels[channelName];
            for (size_t i = 0; i < members.size(); ++i) {
                if (members[i] != senderFd) { 
                    sendMessage(members[i], formattedMessage);
                }
            }
        } else {
            sendMessage(senderFd, "Error: Channel '" + channelName + "' not found.");
        }
    } else {
        std::string lowerRecipientNickname = toLower(trim(recipientNickname));
        int recipientFd = -1;

        for (std::map<int, std::string>::iterator it = clientNicknames.begin(); it != clientNicknames.end(); ++it) {
            if (toLower(trim(it->second)) == lowerRecipientNickname) {
                recipientFd = it->first;
                break;
            }
        }

        if (recipientFd != -1) {
            std::string formattedMessage = ":" + senderNickname + "!" + senderNickname + "@server PRIVMSG " + recipientNickname + " :" + message;
            
            sendMessage(recipientFd, formattedMessage);
        } else {
            sendMessage(senderFd, "Error: User '" + recipientNickname + "' not found.");
        }
    }
}
