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
    // Ensure the channel exists
    if (channels.find(channel) == channels.end()) {
        sendMessage(clientFd, "ERROR :No such channel.");
        return;
    }

    // Check operator status
    if (!isClientOperatorOfChannel(clientFd, channel)) {
        sendMessage(clientFd, "ERROR :You're not channel operator.");
        return;
    }

    // Process mode changes
    if (mode == "i") {
        // Invite-only mode
        channelInviteOnly[channel] = set;
        sendMessage(clientFd, "MODE " + channel + (set ? " +i" : " -i"));
    } else if (mode == "t") {
        // Topic protection mode
        channelOperatorRestrictions[channel] = set;
        sendMessage(clientFd, "MODE " + channel + (set ? " +t" : " -t"));
    } else if (mode == "k") {
        // Password (key) mode
        if (set && !password.empty()) {
            channelPasswords[channel] = password;
            sendMessage(clientFd, "MODE " + channel + " +k");
        } else if (!set) {
            channelPasswords.erase(channel);
            sendMessage(clientFd, "MODE " + channel + " -k");
        } else {
            sendMessage(clientFd, "ERROR :Password required to set +k mode.");
            return;
        }
    } else if (mode == "l") {
        // User limit mode
        if (set) {
            int limit;
            std::istringstream(password) >> limit;
            if (limit > 0) {
                channelUserLimits[channel] = limit;
                sendMessage(clientFd, "MODE " + channel + " +l " + password);
            } else {
                sendMessage(clientFd, "ERROR :Invalid limit for mode +l.");
            }
        } else {
            channelUserLimits.erase(channel);
            sendMessage(clientFd, "MODE " + channel + " -l");
        }
    } else if (mode == "o") {
        // Operator status
        int targetFd = getClientFdFromNickname(targetNickname);
        if (targetFd == -1) {
            sendMessage(clientFd, "ERROR :No such nick/channel.");
        } else {
            if (set) {
                channelOperators[channel] = targetFd;
                sendMessage(clientFd, "MODE " + channel + " +o " + targetNickname);
            } else {
                if (channelOperators[channel] == targetFd) {
                    channelOperators.erase(channel); // Removing operator status
                    sendMessage(clientFd, "MODE " + channel + " -o " + targetNickname);
                } else {
                    sendMessage(clientFd, "ERROR :" + targetNickname + " is not an operator.");
                }
            }
        }
    } else {
        sendMessage(clientFd, "ERROR :Unknown MODE flag");
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
                broadcastMessage(channel, clientNicknames[clientFd] + " has changed the topic to: " + newTopic, clientFd);
            }
        }
    } else {
        sendMessage(clientFd, "Error: Operator restrictions are in place. Only the operator can view or change the topic.");
    }
}

void Server::joinChannel(int clientFd, const std::string& channelName, const std::string& password) {
    // Check for valid channel name
    if (channelName.empty() || channelName[0] != '#') {
        sendMessage(clientFd, "ERROR :Invalid channel name. Channel names must start with '#'.");
        return;
    }

    // Ensure nickname is set
    if (clientNicknames.find(clientFd) == clientNicknames.end() || clientNicknames[clientFd].empty()) {
        sendMessage(clientFd, "ERROR :You must set a nickname before joining a channel.");
        return;
    }

    // Invite-only and password checks
    bool isInviteOnly = channelInviteOnly.find(channelName) != channelInviteOnly.end() && channelInviteOnly[channelName];
    bool isInvited = channelInvitations[channelName].find(clientFd) != channelInvitations[channelName].end();
    if (isInviteOnly && !isInvited) {
        sendMessage(clientFd, "ERROR :" + channelName + " is invite-only, and you're not invited.");
        return;
    }
    if (channelPasswords.find(channelName) != channelPasswords.end() && channelPasswords[channelName] != password) {
        sendMessage(clientFd, "ERROR :Incorrect password for " + channelName + ".");
        return;
    }

    // User limit check
    if (channels.find(channelName) != channels.end()) {
        std::vector<int>& clients = channels[channelName];
        if (channelUserLimits.find(channelName) != channelUserLimits.end() && clients.size() >= channelUserLimits[channelName]) {
            sendMessage(clientFd, "ERROR :User limit for " + channelName + " has been reached.");
            return;
        }
    }

    // Join or create channel
    bool isNewChannel = channels.find(channelName) == channels.end();
    channels[channelName].push_back(clientFd);
    clientLastChannel[clientFd] = channelName;
    
    // Notify client of successful join
    sendMessage(clientFd, "JOIN :" + channelName);
    if (isNewChannel) {
        channelOperators[channelName] = clientFd; // First member becomes operator
        sendMessage(clientFd, "MODE " + channelName + " +o " + clientNicknames[clientFd]); // Notify client of operator status
    }

    // Notify others in the channel
    std::string joinMessage = ":" + clientNicknames[clientFd] + "!~" + clientNicknames[clientFd] + "@client.ip JOIN :" + channelName;
    broadcastMessage(channelName, joinMessage, clientFd);

    // If the channel already had a topic set, send it to the new member
    if (!channelTopics[channelName].empty()) {
        sendMessage(clientFd, "332 " + clientNicknames[clientFd] + " " + channelName + " :" + channelTopics[channelName]);
    }

    // Notify the new client of all members in the channel (including self)
    for (size_t i = 0; i < channels[channelName].size(); ++i) {
        int memberFd = channels[channelName][i];
        sendMessage(clientFd, "353 " + clientNicknames[clientFd] + " = " + channelName + " :" + clientNicknames[memberFd]);
    }
    sendMessage(clientFd, "366 " + clientNicknames[clientFd] + " " + channelName + " :End of /NAMES list.");
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
