#include <map>
#include <vector>
#include <string>
#include "ClientInfo.hpp"
#include <netinet/in.h>
#include <poll.h>

#define MAX_CLIENTS 1024

class Server {
private:
    std::map<int, ClientInfo> clients;
    std::map<int, std::string> clientNicknames;
    std::map<std::string, std::vector<int> > channels; // Map for channel names to lists of participants
    typedef void (Server::*CommandHandler)(int, const std::vector<std::string>&);
    std::map<std::string, CommandHandler> commandHandlers;
    std::map<int, std::string> clientUsernames;
    std::map<int, std::string> clientBuffers;
    std::map<int, std::string> clientLastChannel;
    std::map<std::string, int> channelOperators;
    int server_fd;
    struct sockaddr_in server_addr;
    int portNum;
    std::string serverPassword;
    struct pollfd fds[MAX_CLIENTS];
    int nfds; 

public:
    Server();
    Server(int port, const std::string& password);
    void registerCommandHandlers();
    void handleCommand(int clientFd, const std::string& command);
    void nickCmd(int clientFd, const std::vector<std::string>& args);
    void userCmd(int clientFd, const std::vector<std::string>& args);
    void joinCmd(int clientFd, const std::vector<std::string>& args);
    void privMsgCmd(int clientFd, const std::vector<std::string>& args);
    void processCommand(int clientFd, const std::string& command);
    void sendMessage(int clientFd, const std::string& message);
    void joinChannel(int clientFd, const std::string& channelName);
    void broadcastMessage(const std::string& channelName, const std::string& message);
    void processClientMessage(int clientFd, const std::string& message);
    void sendMessageToChannel(int clientFd, const std::string& channel, const std::string& message);
    void acceptNewConnection();
    void processConnections();
    int setNonBlocking(int fd);
    void run();
    void kickCmd(int clientFd, const std::string& channel, const std::string& targetNickname);
    // Other methods...
};
