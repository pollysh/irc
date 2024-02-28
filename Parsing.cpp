#include <iostream>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>

void parseCommand(int client_fd, const string& command) {
    if (startsWith(command, "NICK")) {
        handleNickCommand(client_fd, command.substr(5));
    } else if (startsWith(command, "USER")) {
        handleUserCommand(client_fd, command.substr(5));
    } else if (startsWith(command, "JOIN")) {
        handleJoinCommand(client_fd, command.substr(5));
    } else if (startsWith(command, "MSG")) {
        handleMessageCommand(client_fd, command.substr(4));
    } else if (startsWith(command, "QUIT")) {
        handleQuitCommand(client_fd);
    } else {
        // Handle unknown command or forward message to channel/users
    }
}

void sendCommand(int client_fd, const string& input) {
    if (input.starts_with("/nick ")) {
        send(client_fd, ("NICK " + input.substr(6)).c_str(), input.length() - 5, 0);
    } else if (input.starts_with("/user ")) {
        send(client_fd, ("USER " + input.substr(6)).c_str(), input.length() - 5, 0);
    } // Add more conditions for other commands
}

