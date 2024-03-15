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

using namespace std;

void Server::removeClientFromAllChannels(int clientFd) {
    for (std::map<std::string, std::vector<int> >::iterator it = channels.begin(); it != channels.end(); ++it) {
        std::vector<int>& clients = it->second;
        std::vector<int>::iterator pos = std::find(clients.begin(), clients.end(), clientFd);
        if (pos != clients.end()) {
            clients.erase(pos);
        }
    }
}

int Server::setNonBlocking(int fd) {
    return fcntl(fd, F_SETFL, O_NONBLOCK);
}

Server::Server(int port, const std::string& password) : portNum(port), serverPassword(password), nfds(1) {

    if (port < MIN_PORT || port > MAX_PORT) {
        std::cerr << "Port number must be between " << MIN_PORT << " and " << MAX_PORT << "." << std::endl;
        exit(-1);
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Error establishing socket." << std::endl;
        exit(-1);
    }

    int yes = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        std::cerr << "Error setting socket options." << std::endl;
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
    
    clientAuthenticated[new_socket] = false;
    
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
    char buffer[BUFFER_SIZE];

    for (int i = 1; i < nfds; i++) {
        if (fds[i].revents & POLLIN) {
            ssize_t nbytes = recv(fds[i].fd, buffer, sizeof(buffer) - 1, 0);

            if (nbytes > 0) {
                buffer[nbytes] = '\0'; // Null-terminate the received data
                clientBuffers[fds[i].fd] += std::string(buffer); // Append to buffer

                // Check for command delimiter (\n)
                size_t pos;
                while ((pos = clientBuffers[fds[i].fd].find("\n")) != std::string::npos) {
                    std::string command = clientBuffers[fds[i].fd].substr(0, pos);
                    processClientMessage(fds[i].fd, command); // Process complete command
                    clientBuffers[fds[i].fd].erase(0, pos + 1); // Remove processed command
                }
            } else if (nbytes == 0) {
                handleClientDisconnection(i); // Handle disconnection
            } else {
                // Error handling. Ensure proper handling of EAGAIN/EWOULDBLOCK and other errors.
                // Specific errno handling strategy depends on your application's requirements.
            }
        }
    }
}

void Server::handleClientDisconnection(int clientIndex) {
    int clientFd = fds[clientIndex].fd;
    std::cout << "Client disconnected." << std::endl;
    removeClientFromAllChannels(clientFd);
    close(clientFd); // Close the socket
    clientAuthenticated.erase(clientFd);
    clientNicknames.erase(clientFd);
    clientBuffers.erase(clientFd); // Clean up the command buffer only once

    // Shift the fds array
    for (int j = clientIndex; j < nfds - 1; j++) {
        fds[j] = fds[j + 1];
    }
    nfds--; 
}
