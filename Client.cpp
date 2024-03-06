#include <iostream>
#include <string>
#include <cstring> // For memset
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> // For close

#define BUFFER_SIZE 1024

using namespace std;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <port> <password>" << endl;
        return -1;
    }

    int portNum = atoi(argv[1]); // Convert port number from string to integer
    string password = argv[2]; // Password from command line
    int clientSocket;
    struct sockaddr_in server_addr;

    // Create the socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "\nError establishing socket..." << endl;
        return -1; // Exit with error
    }

    cout << "\n=> Socket client has been created..." << endl;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(portNum);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Connect to localhost

    // Connect to the server
    if (connect(clientSocket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0) {
        cout << "=> Connected to the server on port number: " << portNum << endl;

        // Send password immediately after connection
        send(clientSocket, password.c_str(), password.length(), 0);

        // Client interaction loop
        cout << "\n=> Enter # to end the connection or commands like NICK <nickname>\n" << endl;
        string message;
        do {
            cout << "Client: ";
            getline(cin, message); // Read message from user

            // Send message or command to server
            if (!message.empty()) {
                send(clientSocket, message.c_str(), message.length(), 0);
                if (message == "#") {
                    break; // Exit if '#' is entered
                }
            }
        } while (true);

    } else {
        cerr << "=> Error in connection to the server." << endl;
        close(clientSocket);
        return -1;
    }

    cout << "\n=> Connection terminated.\nGoodbye...\n";
    close(clientSocket); // Close the socket before exiting
    return 0;
}
