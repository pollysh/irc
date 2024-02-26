#include <iostream>
#include <cstring> // Use <cstring> instead of <string.h> for C++ code
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

using namespace std;

int main() {
    int server_fd, client_socket;
    int portNum = 1500; // The port number on which the server will listen
    bool isExit = false;
    int bufsize = 1024;
    char buffer[bufsize];
    string expectedPassword = "pass"; // The expected password for demonstration

    struct sockaddr_in server_addr;
    socklen_t size;

    // Creating the socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        cout << "\nError establishing socket..." << endl;
        exit(1);
    }
    cout << "\n=> Socket server has been created..." << endl;

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(portNum);

    // Binding the socket
    if ((bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr))) < 0) {
        cout << "=> Error binding connection, the socket has already been established..." << endl;
        return -1;
    }

    size = sizeof(server_addr);
    cout << "=> Looking for clients..." << endl;

    listen(server_fd, 1);

    client_socket = accept(server_fd, (struct sockaddr*)&server_addr, &size);

    // Check if it is valid or not
    if (client_socket < 0) 
        cout << "=> Error on accepting..." << endl;
    else {
        memset(buffer, 0, bufsize); // Clear buffer
        recv(client_socket, buffer, bufsize, 0); // Receive password from client

        // Validate password
        if (expectedPassword == string(buffer)) {
            strcpy(buffer, "=> Server connected...\n");
            send(client_socket, buffer, bufsize, 0);
            cout << "=> Connected with the client, password verified." << endl;
        } else {
            strcpy(buffer, "=> Connection refused: incorrect password.\n");
            send(client_socket, buffer, bufsize, 0);
            cout << buffer;
            close(client_socket);
            isExit = true; // Exit or you could loop back to accept() for a new client
        }
    }

    if (!isExit) {
        cout << "\n=> Enter # to end the connection\n" << endl;

        while (!isExit) {
            cout << "Client: ";
            memset(buffer, 0, bufsize); // Clear buffer
            recv(client_socket, buffer, bufsize, 0);
            cout << buffer << " ";
            if (buffer[0] == '#') {
                isExit = true;
            } else {
                cout << "\nServer: ";
                cin.getline(buffer, bufsize);
                send(client_socket, buffer, strlen(buffer), 0);
                if (buffer[0] == '#') {
                    isExit = true;
                }
            }
        }

        cout << "\n\n=> Connection terminated with IP " << inet_ntoa(server_addr.sin_addr);
        close(client_socket);
        cout << "\nGoodbye..." << endl;
    }

    close(server_fd);
    return 0;
}
