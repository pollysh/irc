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

void Server::sendMessage(int clientFd, const std::string& message) {
    std::string formattedMessage = message;
    if (!message.empty() && message[message.length() - 1] != '\n') {
        formattedMessage += "\n"; // Append a newline character if not present
    }
    if (send(clientFd, formattedMessage.c_str(), formattedMessage.length(), 0) == -1) {
        std::cerr << "Failed to send message to client " << clientFd << ": " << strerror(errno) << std::endl;
    }
}

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

    // Handle the PASS command for authentication
    if (firstWord == "PASS") {
        std::string providedPassword;
        iss >> providedPassword;
        if (providedPassword == serverPassword) {
            clientAuthenticated[clientFd] = true;
            sendMessage(clientFd, "Password accepted. You are now authenticated.");
        } else {
            sendMessage(clientFd, "Incorrect password. Please try again.");
        }
        return; // Early return to stop processing
    }

    // Check if the client is authenticated before processing any other command
    if (!clientAuthenticated[clientFd]) {
        sendMessage(clientFd, "Please authenticate with the PASS command.");
        return; // Block further command processing until authenticated
    }

    // Proceed with other commands only if the client is authenticated
    if (clientAuthenticated[clientFd]) {
        if (firstWord == "JOIN" || firstWord == "PRIVMSG" || firstWord == "NICK" || firstWord == "USER"
            || firstWord == "KICK" || firstWord == "INVITE" || firstWord == "TOPIC" || firstWord == "MODE") {
            processCommand(clientFd, message);
        } else {
            // Check if the client has joined any channel
            std::map<int, std::string>::iterator it = clientLastChannel.find(clientFd);
            if (it != clientLastChannel.end() && !it->second.empty()) {
                sendMessageToChannel(clientFd, it->second, message);
            } else {
                sendMessage(clientFd, "You haven't joined any channel.\n");
            }
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
    
    clientAuthenticated[new_socket] = false; // Mark new clients as unauthenticated
    
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

void Server::processConnections() 
{
    for (int i = 1; i < nfds; i++) 
    {
        if (fds[i].revents & POLLIN) 
        {
            char buffer[BUFFER_SIZE] = {0};
            ssize_t nbytes = recv(fds[i].fd, buffer, BUFFER_SIZE - 1, 0);

            if (nbytes > 0) 
            {
                processClientMessage(fds[i].fd, std::string(buffer));
            } else if (nbytes == 0) 
            {
                std::cout << "Client disconnected." << std::endl;
                close(fds[i].fd);

                // Clean up authentication status and any other client-specific data
                clientAuthenticated.erase(fds[i].fd);

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
