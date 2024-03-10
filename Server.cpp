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
    for (int i = 1; i < nfds; i++) {
        if (fds[i].revents & POLLIN) {
            char buffer[BUFFER_SIZE] = {0};
            ssize_t nbytes = recv(fds[i].fd, buffer, BUFFER_SIZE - 1, 0);

            if (nbytes > 0) {
                processClientMessage(fds[i].fd, std::string(buffer));
            } else if (nbytes == 0) {
                std::cout << "Client disconnected." << std::endl;
                close(fds[i].fd);
                clientAuthenticated.erase(fds[i].fd);
                fds[i] = fds[--nfds];
            } else {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    continue;
                } else {
                    std::cerr << "Error on recv: " << strerror(errno) << " (errno " << errno << ")" << std::endl;
                    close(fds[i].fd);
                    fds[i] = fds[--nfds];
                }
            }
        }
    }
}
