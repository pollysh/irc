#include <map>
#include <vector>
#include <string>
#include <netinet/in.h>
#include <poll.h>
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
#include <set>

#define MAX_CLIENTS 1024
#define MAX_PORT 65535
#define MIN_PORT 1023
#define BUFFER_SIZE 1024

class Server {
private:
    std::map<int, std::string> clientNicknames;
    std::map<std::string, std::vector<int> > channels;
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
    std::map<std::string, std::string> channelTopics;
    std::map<std::string, bool> channelInviteOnly;
    std::map<std::string, std::set<int> > channelInvitations; 
    std::map<std::string, bool> channelOperatorRestrictions; 
    std::map<std::string, std::string> channelPasswords;
    std::map<std::string, int> channelUserLimits;
    std::map<int, bool> clientAuthenticated;

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
    void joinChannel(int clientFd, const std::string& channelName, const std::string& password);
    void broadcastMessage(const std::string& channelName, const std::string& message);
    void processClientMessage(int clientFd, const std::string& message);
    void sendMessageToChannel(int clientFd, const std::string& channel, const std::string& message);
    void acceptNewConnection();
    void processConnections();
    int setNonBlocking(int fd);
    void run();
    void kickCmd(int clientFd, const std::string& channel, const std::string& targetNickname);
    void inviteCmd(int clientFd, const std::string& channel, const std::string& targetNickname);
    void topicCmd(int clientFd, const std::string& channel, const std::string& topic);
    void modeCmd(int clientFd, const std::string& channel, const std::string& mode, bool set, const std::string& password = "", const std::string& targetNickname = "");
    void sendPrivateMessage(int senderFd, const std::string& recipientNickname, const std::string& message);
    int getClientFdFromNickname(const std::string& targetNickname);
    bool isClientOperatorOfChannel(int clientFd, const std::string& channel);
};
