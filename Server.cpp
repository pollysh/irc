#include <iostream>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>
#include <map>
#include <sstream>
#include "Server.hpp"

#define MAX_CLIENTS 1024
#define BUFFER_SIZE 1024

using namespace std;

void Server::broadcastMessage(const std::string& channelName, const std::string& message) {
    if (channels.find(channelName) != channels.end()) {
        std::vector<int>& clients = channels[channelName];
        for (size_t i = 0; i < clients.size(); ++i) {
            sendMessage(clients[i], message);
        }
    }
}

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
        std::string channel, modeArg;
        iss >> channel >> modeArg;
    // Checking if setting (+) or removing (-) a mode
        bool set = (modeArg[0] == '+');
        std::string mode = modeArg.substr(1, 1); // Extract the mode character (e.g., 'k' or 'l')
        std::string argument; // Could be a password or user limit

        if (mode == "k") {
        // If setting the password, extract it from the command
            if (set) {
            // Attempt to extract password as a separate argument
                if (!(iss >> argument) || argument.empty()) {
                    sendMessage(clientFd, "Error: A password is required to set the channel key.");
                    return;
                }
            }
            modeCmd(clientFd, channel, mode, set, argument);
        } else if (mode == "l") {
        // Handle user limit mode
            if (set) {
            // Extract the user limit as an argument if setting
                if (!(iss >> argument) || argument.empty()) {
                    sendMessage(clientFd, "Error: A user limit is required to set this mode.");
                    return;
                }
            }
        // Note: For setting the 'l' mode, 'argument' should be the limit, for removing it can be ignored
            modeCmd(clientFd, channel, mode, set, argument);
        } else {
        // Handling other modes without additional data
            modeCmd(clientFd, channel, mode, set);
        }
    }
}

void Server::sendMessageToChannel(int clientFd, const std::string& channel, const std::string& message) {
    if (channels.find(channel) == channels.end()) {
        sendMessage(clientFd, "Channel does not exist or you haven't joined any channel.");
        return;
    }

    const std::vector<int>& members = channels[channel];
    for (std::vector<int>::const_iterator it = members.begin(); it != members.end(); ++it) {
        int memberId = *it;
        if (memberId != clientFd) { // Don't send the message back to the sender
            sendMessage(memberId, message);
        }
    }
}


void Server::processClientMessage(int clientFd, const std::string& message) {
    std::istringstream iss(message);
    std::string firstWord;
    iss >> firstWord;

    // Check if the first word is a recognized command
    if (firstWord == "JOIN" || firstWord == "PRIVMSG" || firstWord == "NICK" || firstWord == "USER" || firstWord == "KICK" || firstWord == "INVITE" || firstWord == "TOPIC" || firstWord == "MODE") {
        // Process known commands
        processCommand(clientFd, message);
    } else {
        // Before sending a message to the last joined channel, check if the client has joined any channel
        if (clientLastChannel.find(clientFd) != clientLastChannel.end() && !clientLastChannel[clientFd].empty()) {
            // Send the message to the last channel the client has joined
            sendMessageToChannel(clientFd, clientLastChannel[clientFd], message);
        } else {
            // If the client hasn't joined any channel, notify them
            sendMessage(clientFd, "You haven't joined any channel.\n");
        }
    }
}

int Server::setNonBlocking(int fd) {
    int flags;
    if ((flags = fcntl(fd, F_GETFL, 0)) == -1) flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

Server::Server(int port, const std::string& password) : portNum(port), serverPassword(password), nfds(1) {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Error establishing socket." << std::endl;
        exit(-1);
    }

    if (setNonBlocking(server_fd) < 0) {
        std::cerr << "Error setting server socket to non-blocking." << std::endl;
        exit(-1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(portNum);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error binding socket." << std::endl;
        exit(-1);
    }

    if (listen(server_fd, 10) < 0) {
        std::cerr << "Error listening on socket." << std::endl;
        exit(-1);
    }

    memset(fds, 0, sizeof(fds));
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    std::cout << "Server started on port " << portNum << " with password protection." << std::endl;
}

void Server::acceptNewConnection() {
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    int new_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
    if (new_socket < 0) {
        std::cerr << "Error accepting new connection." << std::endl;
        return;
    }

    if (setNonBlocking(new_socket) < 0) {
        std::cerr << "Error setting new socket to non-blocking." << std::endl;
        close(new_socket);
        return;
    }

    fds[nfds].fd = new_socket;
    fds[nfds].events = POLLIN;
    nfds++;
    std::cout << "New connection accepted." << std::endl;
}

void Server::run() {
    while (true) {
        int poll_count = poll(fds, nfds, -1);

        if (poll_count < 0) {
            std::cerr << "Poll error." << std::endl;
            continue;
        }

        if (fds[0].revents & POLLIN) {
            acceptNewConnection();
        }

        processConnections();
    }
}

void Server::processConnections() {
    for (int i = 1; i < nfds; i++) {
        if (fds[i].revents & POLLIN) {
            char buffer[BUFFER_SIZE] = {0};
            ssize_t nbytes = recv(fds[i].fd, buffer, BUFFER_SIZE - 1, 0);

            if (nbytes > 0) {
                processClientMessage(fds[i].fd, std::string(buffer));
            } else if (nbytes == 0) {
                std::cout << "Client disconnected." << std::endl;
                close(fds[i].fd);
                fds[i] = fds[--nfds]; // Adjust for removal
            } else {
                std::cerr << "Error on recv." << std::endl;
                close(fds[i].fd);
                fds[i] = fds[--nfds]; // Adjust for removal
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
        return 1;
    }

    int port = std::atoi(argv[1]);
    std::string password = argv[2];
    Server server(port, password);
    server.run();
    return 0;
}
