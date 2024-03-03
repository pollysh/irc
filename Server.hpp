#include <map>
#include <vector>
#include <string>
#include "ClientInfo.hpp"


class Server {
private:
    std::map<int, ClientInfo> clients;
    std::map<int, std::string> clientNicknames;
    std::map<std::string, std::vector<int> > channels; // Map for channel names to lists of participants
    typedef void (Server::*CommandHandler)(int, const std::vector<std::string>&);
    std::map<std::string, CommandHandler> commandHandlers;
    std::map<int, std::string> clientUsernames;

public:
    Server();
    void registerCommandHandlers();
    void handleCommand(int clientFd, const std::string& command);
    void nickCmd(int clientFd, const std::vector<std::string>& args);
    void userCmd(int clientFd, const std::vector<std::string>& args);
    void joinCmd(int clientFd, const std::vector<std::string>& args);
    void privMsgCmd(int clientFd, const std::vector<std::string>& args);
    void processCommand(int clientFd, const std::string& command);
    void sendMessage(int clientFd, const std::string& message);
    // Other methods...
};
