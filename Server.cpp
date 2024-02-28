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

#define MAX_CLIENTS 1024

using namespace std;

map<int, string> clientNicknames; // Map to associate client FDs with nicknames

int setNonBlocking(int fd) {
    int flags;
    if ((flags = fcntl(fd, F_GETFL, 0)) == -1) flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void processCommand(int clientFd, const string& command) {
    istringstream iss(command);
    string cmd;
    iss >> cmd; // Extract the command itself

    if (cmd == "NICK") {
        string nickname;
        iss >> nickname; // Extract the nickname
        if (!nickname.empty()) {
            clientNicknames[clientFd] = nickname; // Update the client's nickname
            cout << "Client " << clientFd << " sets nickname to " << nickname << endl;
            // Optionally, send a confirmation message back to the client
        }
    }
    // Additional commands can be handled here in a similar manner
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cout << "Usage: ./ircserv <port> <password>" << endl;
        return 1;
    }

    int portNum = atoi(argv[1]);
    string expectedPassword = argv[2];
    int server_fd, client_socket, activity;
    struct sockaddr_in server_addr;
    socklen_t addr_len;
    char buffer[1024];
    struct pollfd fds[MAX_CLIENTS];
    int nfds = 1;

    memset(fds, 0, sizeof(fds));
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        cerr << "Error establishing socket..." << endl;
        exit(EXIT_FAILURE);
    }

    setNonBlocking(server_fd);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(portNum);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Error binding connection..." << endl;
        exit(EXIT_FAILURE);
    }

    listen(server_fd, 10);
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    cout << "Server started on port " << portNum << " with password protection." << endl;

    while (true) {
        activity = poll(fds, nfds, -1);

        if (activity < 0) {
            cerr << "Poll error." << endl;
            continue;
        }

        if (fds[0].revents & POLLIN) {
            client_socket = accept(server_fd, (struct sockaddr *)&server_addr, &addr_len);
            if (client_socket < 0) {
                cerr << "Accept error." << endl;
                continue;
            }

            setNonBlocking(client_socket);
            memset(buffer, 0, 1024);
            ssize_t nbytes = recv(client_socket, buffer, 1024, 0);

            if (nbytes > 0) {
                buffer[nbytes] = '\0';
                cout << "Received password: '" << buffer << "'" << endl;
                if (expectedPassword == string(buffer)) {
                    cout << "Password match. Client authenticated." << endl;
                    fds[nfds].fd = client_socket;
                    fds[nfds].events = POLLIN;
                    nfds++;
                    cout << "New connection accepted." << endl;
                } else {
                    cout << "Connection refused: incorrect password." << endl;
                    close(client_socket);
                }
            } else {
                cerr << "Error receiving password or client disconnected." << endl;
                close(client_socket);
            }
        }

        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                memset(buffer, 0, 1024);
                int nbytes = recv(fds[i].fd, buffer, 1024, 0);

                if (nbytes <= 0) {
                    if (nbytes == 0) {
                        cout << "Socket " << fds[i].fd << " disconnected." << endl;
                    } else {
                        cerr << "Recv error." << endl;
                    }
                    close(fds[i].fd);
                    fds[i] = fds[nfds - 1];
                    nfds--;
                    i--;
                } else {
                    buffer[nbytes] = '\0';
                    cout << "Message from client " << fds[i].fd << ": " << buffer << endl;
                    processCommand(fds[i].fd, string(buffer));
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
