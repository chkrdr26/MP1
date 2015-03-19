//
//  Message.h
//  CentralServer
//
//  messages in the communication channels and central servers
//  with several attributes, like the origin and destination
//  nodes, the time the message sent out and message content

#include <time.h>

class Message {
public:
    char content[50];
    char from[2];
    char to[2];
    time_t sent_time;
};