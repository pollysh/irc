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
#include <errno.h>
#include <algorithm>
#include <iomanip>
#include <sstream>

#define MAX_CLIENTS 1024
#define MAX_PORT 65535
#define MIN_PORT 1023
#define BUFFER_SIZE 1024
const int ERR_NOSUCHCHANNEL = 403;
const int RPL_NOTOPIC = 331;
const int RPL_TOPIC = 332;
const int ERR_CHANOPRIVSNEEDED = 482;
#define RPL_TOPIC 332
#define RPL_NAMREPLY 353
#define RPL_ENDOFNAMES 366
#define ERR_NOSUCHNICK 401
#define ERR_NOSUCHCHANNEL 403
#define ERR_CANNOTSENDTOCHAN 404
#define ERR_NONICKNAMEGIVEN 431
#define ERR_ERRONEUSNICKNAME 432
#define ERR_NICKNAMEINUSE 433
#define ERR_NOTONCHANNEL 442
#define ERR_NOTREGISTERED 451
#define RPL_TOPIC 332
#define RPL_NAMREPLY 353
#define RPL_ENDOFNAMES 366


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
    std::map<int, std::vector<std::string> > clientChannels;
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
    std::map<int, int> authFlags;
    std::map<int, std::string> partialInputs;

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
    void broadcastMessage(const std::string& channelName, const std::string& message, int excludeFd);
    void processClientMessage(int clientFd, const std::string& rawMessage);
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
    bool nickCmd(int clientFd, const std::string& command);
    void userCmd(int clientFd, const std::string& command);
    void receiveData(int clientFd, const std::string& data);
    std::string trim(const std::string& str);
    void showMessage(const std::string& channelName, const std::string& message);
    void sendNumericReply(int clientFd, int numericCode, const std::string& message);
    std::string formatMessage(const std::string &senderNickname, const std::string &message);
    bool processInitialCommand(int clientFd, const std::string &command, std::istringstream &iss);
    void sendToLastJoinedChannel(int clientFd, const std::string& message);
    void redirectMessageToOtherChannel(int clientFd, const std::string& message);
    std::string formatMessageForChannel(int clientFd, const std::string& channel, const std::string& message);
};
