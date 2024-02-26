#include <iostream>
#include <string>
#include <cstring> // for memset and memcpy
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>

using namespace std;

int main() {
    int client;
    int portNum = 1500; // Assuming the port is fixed for simplicity
    bool isExit = false;
    int bufsize = 1024;
    char buffer[bufsize];
    string ip = "127.0.0.1"; // Server IP address
    string password = "pass"; // Hardcoded password for demonstration

    struct sockaddr_in server_addr;

    client = socket(AF_INET, SOCK_STREAM, 0);
    if (client < 0) {
        cout << "\nError establishing socket..." << endl;
        exit(1);
    }

    cout << "\n=> Socket client has been created..." << endl;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(portNum);
    memset(&server_addr.sin_zero, 0, sizeof(server_addr.sin_zero)); // Ensure the struct is empty
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    if (connect(client, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0) {
        cout << "=> Connection to the server port number: " << portNum << endl;

        // Send password immediately after establishing the connection
        send(client, password.c_str(), password.length(), 0);

        // Await confirmation from the server
        cout << "=> Awaiting confirmation from the server..." << endl;
        memset(buffer, 0, bufsize); // Clear the buffer before receiving
        recv(client, buffer, bufsize, 0);
        cout << "Server: " << buffer << endl;
    } else {
        cout << "=> Error in connection to server." << endl;
        return -1;
    }

    // Main communication loop
    cout << "\n\n=> Enter # to end the connection\n" << endl;
    do {
        cout << "Client: ";
        cin.getline(buffer, bufsize);
        send(client, buffer, strlen(buffer), 0);
        if (buffer[0] == '#') {
            isExit = true;
        }

        if (!isExit) {
            cout << "Server: ";
            recv(client, buffer, bufsize, 0);
            cout << buffer << endl;
            if (buffer[0] == '#') {
                isExit = true;
            }
        }
    } while (!isExit);

    cout << "\n=> Connection terminated.\nGoodbye...\n";
    close(client);
    return 0;
}
