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

void Server::joinChannel(int clientFd, const std::string& channelName) {
    if (clientNicknames.find(clientFd) == clientNicknames.end() || clientNicknames[clientFd].empty()) {
        std::cerr << "Client " << clientFd << " attempted to join a channel without setting a nickname." << std::endl;
        sendMessage(clientFd, "You must set a nickname before joining a channel.");
        return;
    }

    if (channelName.empty() || channelName[0] != '#') {
        std::cerr << "Client " << clientFd << " attempted to join an invalid channel name: " << channelName << std::endl;
        sendMessage(clientFd, "Invalid channel name. Channel names must start with a '#'.");
        return;
    }
    
    // Check if the channel exists, if not, create it
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

    // If this is the first client to join the channel, make them the operator
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

void Server::kickCmd(int clientFd, const std::string& channel, const std::string& targetNickname) {
    // Check if clientFd is the operator of the channel
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

        // Check if the target user is in the channel
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
        iss >> channel;
        joinChannel(clientFd, channel);
    } else if (cmd == "PRIVMSG") {
        std::string target;
        std::getline(iss, target, ' ');
        
        std::string message;
        std::getline(iss, message);
        
        if (target.empty() || target[0] != '#') {
            if (clientLastChannel.find(clientFd) != clientLastChannel.end() && !clientLastChannel[clientFd].empty()) {
                target = clientLastChannel[clientFd];
            } else {
                sendMessage(clientFd, "You have not joined any channel.");
            }
        } else
        {
            if (!message.empty()) {
            // Send the message to the specified channel
            sendMessageToChannel(clientFd, target, message);
            }
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
    if (firstWord == "JOIN" || firstWord == "PRIVMSG" || firstWord == "NICK" || firstWord == "USER" || firstWord == "KICK") {
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
