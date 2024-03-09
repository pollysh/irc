#include "Server.hpp"

void Server::modeCmd(int clientFd, const std::string& channel, const std::string& mode, bool set, const std::string& password) {
    // Existing checks for channel existence and operator status...

    if (mode == "i") {
        channelInviteOnly[channel] = set;
        std::string modeStatus = set ? "enabled" : "disabled";
        sendMessage(clientFd, "Invite-only mode for " + channel + " has been " + modeStatus + ".");
        
        // Notify all channel members about the mode change
        broadcastMessage(channel, "The channel " + channel + " is now in invite-only mode: " + modeStatus);
    } else if (mode == "t") {
        // Handle operator restrictions mode
        channelOperatorRestrictions[channel] = set;
        std::string status = set ? "enabled" : "disabled";
        sendMessage(clientFd, "Operator restrictions for " + channel + " are now " + status + ".");
        broadcastMessage(channel, "The channel " + channel + " now has operator restrictions " + status + ".");
    } else if (mode == "k") {
        // Handle setting or removing the channel password
        if (set && !password.empty()) {
            // Set the channel password
            channelPasswords[channel] = password;
            sendMessage(clientFd, "Password for " + channel + " has been set.");
            broadcastMessage(channel, "A password is now required to join " + channel + ".");
        } else if (!set) {
            // Remove the channel password
            channelPasswords.erase(channel);
            sendMessage(clientFd, "Password for " + channel + " has been removed.");
            broadcastMessage(channel, channel + " no longer requires a password to join.");
        } else {
            sendMessage(clientFd, "Error: A password is required to set the channel key.");
        }
    } else if (mode == "l") {
        if (set) {
            try {
                int limit = std::stoi(password); // Treat the password string as the limit for the 'l' mode
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
    } else {
        sendMessage(clientFd, "Error: Unsupported mode.");
    }
}

void Server::topicCmd(int clientFd, const std::string& channel, const std::string& topic) {
    // Check if the channel exists
    if (channels.find(channel) == channels.end()) {
        sendMessage(clientFd, "Error: The specified channel does not exist.");
        return;
    }

    // Check if the client is a member of the channel
    std::vector<int>& clients = channels[channel];
    if (std::find(clients.begin(), clients.end(), clientFd) == clients.end()) {
        sendMessage(clientFd, "Error: You are not a member of the specified channel.");
        return;
    }

    // If no topic argument is provided, display the current topic
    if (topic.empty()) {
        std::string currentTopic = channelTopics[channel];
        if (currentTopic.empty()) {
            sendMessage(clientFd, "No topic is set for " + channel + ".");
        } else {
            sendMessage(clientFd, "Current topic for " + channel + ": " + currentTopic);
        }
    } else {
        // Check if the client is the channel operator
        if (channelOperators.find(channel) != channelOperators.end() && channelOperators[channel] == clientFd) {
            // Update the channel's topic
            channelTopics[channel] = topic;
            sendMessage(clientFd, "Topic for " + channel + " updated to: " + topic);
            // Optionally, notify all channel members about the topic change
            broadcastMessage(channel, "The topic for " + channel + " has been changed to: " + topic);
        } else {
            sendMessage(clientFd, "Error: Only the channel operator can change the topic.");
        }
    }
}

void Server::joinChannel(int clientFd, const std::string& channelName, const std::string& password) {

    bool isInviteOnly = channelInviteOnly[channelName];
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
        
        // Check if a user limit is set and reached
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
        // If the channel already exists, check if the client is already in the channel
        std::vector<int>& clients = channels[channelName];
        if (std::find(clients.begin(), clients.end(), clientFd) != clients.end()) {
            // Client is already in the channel, notify them and return without adding them again
            sendMessage(clientFd, "You are already a member of " + channelName);
            return;
        }
    }

    // Add the client to the channel if they are not already a member
    channels[channelName].push_back(clientFd);
    clientLastChannel[clientFd] = channelName; 
    std::cout << "Client " << clientFd << " with nickname " << clientNicknames[clientFd] << " joined channel " << channelName << std::endl;
    sendMessage(clientFd, "Welcome to " + channelName + "!");

    
    if (channels[channelName].size() == 1) {
        channelOperators[channelName] = clientFd;
        sendMessage(clientFd, "You are the operator of " + channelName + ".");
    }

    // Notify other members of the channel
    for (size_t i = 0; i < channels[channelName].size(); ++i) {
        int memberFd = channels[channelName][i];
        if (memberFd != clientFd) {
            sendMessage(memberFd, clientNicknames[clientFd] + " has joined the channel.");
        }
    }
}

void Server::inviteCmd(int clientFd, const std::string& channel, const std::string& targetNickname) {
    if (channels.find(channel) == channels.end()) {
        sendMessage(clientFd, "Error: The specified channel does not exist.");
        return;
    }

    if (channelOperators.find(channel) != channelOperators.end() && channelOperators[channel] == clientFd) {
        // The user issuing the command is the channel operator
        // Now find the target user's file descriptor (fd) based on the nickname
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

        // Check if the target user is already in the channel
        std::vector<int>& clients = channels[channel];
        if (std::find(clients.begin(), clients.end(), targetFd) != clients.end()) {
            sendMessage(clientFd, "Error: User already in the channel.");
            return;
        }

        // Invite the user to the channel
        sendMessage(targetFd, "You have been invited to join " + channel + ". Use JOIN command to enter.");
        sendMessage(clientFd, "Invitation sent to " + targetNickname + ".");
    } else {
        // The user issuing the command is not the channel operator
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
            // Remove the user from the channel
            clients.erase(std::remove(clients.begin(), clients.end(), targetFd), clients.end());
            sendMessage(targetFd, "You have been kicked from " + channel + ".");
            broadcastMessage(channel, targetNickname + " has been kicked from the channel.");
        } else {
            sendMessage(clientFd, "Error: User not in the specified channel.");
        }
    } else {
        // The user issuing the command is not the channel operator
        sendMessage(clientFd, "Error: Only the channel operator can kick users.");
    }
}

void Server::sendPrivateMessage(int senderFd, const std::string& recipientNickname, const std::string& message) {
    int recipientFd = -1;
    // Explicitly use iterator for map iteration
    for (std::map<int, std::string>::iterator it = clientNicknames.begin(); it != clientNicknames.end(); ++it) {
        if (it->second == recipientNickname) {
            recipientFd = it->first;
            break;
        }
    }

    if (recipientFd != -1) {
        // Found the recipient, send the message
        std::string senderNickname = clientNicknames[senderFd];
        sendMessage(recipientFd, "Private message from " + senderNickname + ": " + message);
    } else {
        // Recipient not found
        sendMessage(senderFd, "Error: User '" + recipientNickname + "' not found.");
    }
}