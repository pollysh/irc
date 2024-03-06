// ClientInfo.h
#ifndef CLIENT_INFO_H
#define CLIENT_INFO_H

#include <string>
using std::string;

struct ClientInfo {
    string nickname;
    string username;
    std::string lastChannelJoined;
    // Add other attributes here as needed
};

#endif // CLIENT_INFO_H
