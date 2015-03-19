//
//  Node.cpp
//  Nodes
//
//  a distributed key-value storage system over a network with delays
//  each node represents a server in this key-value distribtued system
//  each node may potentially store the value for each key in the shared datastore

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <time.h>
#include <thread>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <queue>
#include <vector>
#include "Operation.h"
#include "Compare.h"

#define SERVER_ADDR "127.0.0.1"

int sockfd;
int ports[4];
char name;
int seq = 0;                                                              //the biggest sequence number this node has been received
std::priority_queue<char *, std::vector<Operation*>, Compare> buffer;     //data structure for buffering the messages with sequences
std::unordered_map<int, int> kv_store;                                    //data structure for key-value store
std::unordered_map<int, int> kc_store;
std::unordered_map<int, std::string> kr_store;
std::unordered_map<int, long> kw_store;


//parse string like 1_A_22_Update 5 8
void parse(char *buf, Operation* op)
{
    std::string buff = buf;
    std::istringstream iss(buff);
    char tmp[2];
    iss >>tmp;
    iss >> op->from;
    iss>> op->seq_num;
    iss>> op->oper;
    iss >> op->key;
    if (op->oper[0] == 'g' || op->oper[0] == 'a') {
        iss >> op->model;
    } else if (op->oper[0] == 'i' || op->oper[0] == 'u'){
        iss >> op->value;
        iss >> op->model;
    } else if (op->oper[0] == 'e'){
        iss >> op->model;
        iss >> op->lastWrite;
    } else if (op->oper[0] == 'r'){
        iss >> op->model;
        iss >> op->value;
        iss >> op->lastWrite;
    }
}


//for thread that receives messages from central server
void receiver_func(int max_delay)
{
    time_t curr_time;
    while(1)
    {
        char buf[50];
        if (recv(sockfd, buf, sizeof(buf), 0) == -1)
        {
            perror("recv");
            exit(1);
        }
        
        // received buf: "0_B_hello", which means node B sent "Hello" to me
        if (buf[0] == '0') {
            char sender[2];
            char content[50];
            time(&curr_time);
            strtok(buf, "_");
            strcpy(sender, strtok(NULL, "_"));
            strcpy(content, strtok(NULL, "_"));
            printf("received \"%s\" from %c, Max delay is %d s, system time is %d\n", content, sender[0], max_delay, (int)curr_time);
            
        } else if (buf[0] == '2') {
            Operation* op = new Operation();
            parse(buf, op);
            //printf("received operation \"%s\" with seq# %d from %c\n", op->oper, op->seq_num, op->from[0]);
            buffer.push(op);
            while (!buffer.empty() && buffer.top()->seq_num == seq + 1) {
                Operation* op = buffer.top();
                if (strcmp(op->oper, "insert") == 0){
                    std::pair<int, int> pv(op->key, op->value);
                    kv_store.insert(pv);
                    //printf("inserted (%d, %d) to the key value store\n", op->key, op->value);
                    
                    time_t ptime;
                    time(&ptime);
                    std::pair<int, long> pw(op->key, ptime);
                    kw_store.insert(pw);
                    
                    auto it = kr_store.find(op->key);
                    if (it == kr_store.end()) {
                        kr_store.insert(std::pair<int, std::string>(op->key, " "));
                    }
                    
//                    auto gt = kc_store.find(op->seq_num);
//                    if (gt == kc_store.end()) {
//                        kc_store.insert(std::pair<int, int>(op->key, 0));
//                    }
                    
                    char cmsg[50];
                    sprintf(cmsg, "6 %d ack %d %d", op->seq_num, op->key, op->model);
                    if (send(sockfd, cmsg, 30, 0) == -1)
                        perror("send");
                    
                    if (op->model > 2) {
                        sprintf(cmsg, "7 %c %d even %d %d %ld", op->from[0], op->seq_num, op->key, op->model, op->lastWrite);
                        if (send(sockfd, cmsg, 30, 0) == -1)
                            perror("send");
                    }
                    
                    
                    
                } else if (strcmp(op->oper, "update") == 0) {
                    auto it = kv_store.find(op->key);
                    if (it == kv_store.end()) {
                        printf("there is no key %d\n", op->key);
                    } else {
                        it->second = op->value;
                        //printf("execute operation \"%s (%d,%d)\" with seq# %d from %c\n", op->oper, op->key, op->value, op->seq_num, op->from[0]);
                        time_t ptime;
                        time(&ptime);
                        kw_store.find(op->key)->second = ptime;
                        
                    }
                    char cmsg[50];
                    sprintf(cmsg, "6 %d ack %d %d", op->seq_num, op->key, op->model);
                    if (send(sockfd, cmsg, 30, 0) == -1)
                        perror("send");
                    
                    if (op->model > 2) {
                        sprintf(cmsg, "7 %c %d even %d %d %ld", op->from[0], op->seq_num, op->key, op->model, op->lastWrite);
                        if (send(sockfd, cmsg, 30, 0) == -1)
                            perror("send");
                    }
                } else if (strcmp(op->oper, "get") == 0) {
                    auto it = kv_store.find(op->key);

                    if (op->model < 3) {
                        if (it == kv_store.end()) {
                            printf("there is no key %d\n", op->key);
                        } else if (op->from[0] == name) {
                            //printf("execute operation \"%s %d\" with seq# %d from %c\n", op->oper, op->key, op->seq_num, op->from[0]);
                            printf("value of key %d is %d\n", it->first, it->second);
                        }
                    
                    } else {
                        if (it == kv_store.end()) {
                            printf("there is no key %d\n", op->key);
                        } else {
                        char cmsg[50];

                        sprintf(cmsg, "7 %c %d reven %d %d %d %ld", op->from[0], op->seq_num, op->key, op->model, it->second, op->lastWrite);
                        if (send(sockfd, cmsg, 30, 0) == -1)
                            perror("send");
                        }
                    }
                    
                } else {
                    
                    
                }
                seq ++;
                buffer.pop();
            }
        } else if (buf[0] == '6') {
            Operation* op = new Operation();
            parse(buf, op);
            //printf("got ack from %s for key %d\n", op->from, op->key);
            auto it = kr_store.find(op->key);
            if (it != kr_store.end()) {
                it->second.append(&op->from[0]);
            } else{
                std::string ks = " ";
                ks.append(&op->from[0]);
                kr_store.insert(std::pair<int, std::string>(op->key, ks));
            }
            if (op->from[0] == name && op->model < 3) {
                printf("write acknowledged\n");
            }
            
            
        } else if (buf[0] == '7') {
            Operation* op = new Operation();
            parse(buf, op);
            //printf("got ack from %s for key %d\n", op->from, op->key);
            auto it = kc_store.find(op->seq_num);
            if (it != kc_store.end()) {
                it->second++;
                printf("testing eventual %d\n", it->second);
            } else {
                kc_store.insert(std::pair<int, int>(op->seq_num, 0));
            }
            if (strcmp(op->oper, "reven") == 0) {
                if (kw_store.find(op->key)->second < op->lastWrite) {
                    kw_store.find(op->key)->second = op->lastWrite;
                    kv_store.find(op->key)->second = op->value;
                }
            }
            if (kc_store.find(op->seq_num)->second == op->model - 2) {
                if (strcmp(op->oper, "reven") == 0) {
                    printf("value of key %d is %d\n", op->key, kv_store.find(op->key)->second);
                } else {
                    printf("write request acknowledged\n");
                }
            }
            
            
        } else if (buf[0] == '3'){
            
            std::string str = buf;
            std::istringstream iss(str);
            int key;
            char tmp[5];
            iss >> tmp;
            iss >> tmp;
            iss >> key;
            kv_store.erase(key);
            kr_store.erase(key);
            kw_store.erase(key);
            
        }
    }
}

//read configuration file to get ports and max delay for channels
void read_config(int &max_delay)
{
    int i = 0;
    std::string line;
    std::ifstream config("/Users/Chakri/Documents/config.txt");
    if (config.is_open())
    {
        getline(config, line);
        max_delay = stoi(line);
        while (getline(config, line))
        {
            ports[i++] = stoi(line);
        }
        config.close();
    }
    else printf("Unable to open file");
}

//main thread that read command line and send requests to central server
int main(int argc, char * argv[])
{
    struct sockaddr_in dest;
    time_t start_time, last_cmd_time = 0, cmd_delay = 0, delay;
    int max_delay;
    
    read_config(max_delay);
    
    //open socket for streaming
    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
    {
        perror("Socket");
        exit(errno);
    }
    
    //initialize server address/port struct
    bzero(&dest, sizeof(dest));
    dest.sin_family = AF_INET;
//    dest.sin_port = htons(9000);
//    name = 9000 - ports[0] + 'A';
    dest.sin_port = htons(atoi(argv[1]));
    name = atoi(argv[1]) - ports[0] + 'A';
    if ( inet_aton(SERVER_ADDR, (in_addr *) &dest.sin_addr.s_addr) == 0 )
    {
        perror(SERVER_ADDR);
        exit(errno);
    }
    
    //connect to server
    if ( connect(sockfd, (struct sockaddr *) &dest, sizeof(dest)) != 0 )
    {
        perror("Connect ");
        exit(errno);
    }
    
    std::thread receiver(receiver_func, max_delay);
    
    while(1)
    {
        char input[50], temp[50], content[40], dest[2], key[10], value[10], cons_model[2];        //arrays for storing tokens
        
        if (fgets(input, sizeof(input), stdin))
        {
            strcpy(temp, input);
            time(&start_time);
            
            if (cmd_delay && start_time - last_cmd_time < cmd_delay)                              //control delay between operations
            {
                printf("time elapse from last operation is less than specified delay, wait please \n");
                continue;
            }
            
            char *token = strtok (temp, " \n");
            if (strcmp(token, "send") == 0)                                                       //send Hello B
            {
                strcpy(content, strtok (NULL, " "));
                strcpy(dest, strtok(NULL, "\n"));
                printf("sent \"%s\" to %s, system time is %d\n", content, dest, (int)start_time);
                if (send(sockfd, input, sizeof(input), 0) == -1)
                    perror("send");
            } else if (strcmp(token, "insert") == 0 || strcmp(token, "update") == 0) {            //insert 1 1 1, update 1 2 1
                strcpy(key, strtok (NULL, " "));
                strcpy(value, strtok(NULL, " "));
                strcpy(cons_model, strtok(NULL, "\n"));
                strcat(token, " ");
                strcat(token, key);
                strcat(token, " ");
                strcat(token, value);
                strcat(token, " ");
                strcat(token, cons_model);
                if (send(sockfd, token, strlen(token), 0) == -1)
                    perror("send");
            } else if (strcmp(token, "get") == 0) {
                strcpy(key, strtok (NULL, " "));
                strcpy(cons_model, strtok(NULL, "\n"));
                strcat(token, " ");
                strcat(token, key);
                strcat(token, " ");
                strcat(token, cons_model);
                if (strcmp(cons_model, "1") == 0) {                                               //linearizability
                    if (send(sockfd, token, strlen(token), 0) == -1)
                        perror("send");
                } else if (strcmp(cons_model, "2") == 0) {                                        //sequential consistency
                    printf("value of key %d is %d\n", atoi(key), kv_store[atoi(key)]);
                } else {
                    if (send(sockfd, token, strlen(token), 0) == -1)
                        perror("send");
                }                                                                         //eventual consistency, W = 1, R = 1 and W = 2, R = 2
            } else if (strcmp(token, "delete") == 0) {                                            //delete 1
                strcpy(key, strtok (NULL, " "));
                strcat(token, " ");
                strcat(token, key);
                kv_store.erase(atoi(key));
                if (send(sockfd, token, strlen(token), 0) == -1)
                    perror("send");
                
            } else if (strcmp(token, "show-all") == 0) {                                          //show-all displays all the key-value pairs stored on the server performing this command
                printf("all key-value pairs stored on this server:\n");
                for ( auto it = kv_store.begin(); it != kv_store.end(); ++it )
                    printf("(%d,%d)\n", it->first, it->second);
            } else if (strcmp(token, "search") == 0) {                                            //search displays the list of servers that store replicas of the key being searched
                strcpy(key, strtok (NULL, " "));
                auto it = kv_store.find(atoi(key));
                if (it == kv_store.end()) {
                    printf("can't find key\n");
                } else{
                    printf("found in nodes %s\n", kr_store.find(atoi(key))->second.c_str());
                }
                
            } else if (strcmp(token, "delay") == 0) {                                             //delay between operations invoked at each server
                char delay[5];
                strcpy(delay, strtok (NULL, "\n"));
                cmd_delay = atoi(delay);
                continue;
            } else if (strcmp(token, "quit") == 0) {                                              //quit program
                receiver.join();
                //close(sockfd);
                return 0;
            } else {                                                                              //input format is wrong
                printf("input format is wrong, please input again:\n");
            }
            cmd_delay = 0;
            time(&last_cmd_time);
        }
    }
}


