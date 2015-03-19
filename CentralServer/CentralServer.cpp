//
//  CentralServer.cpp
//  CentralServer
//
//  central server is mainly for:
//  1. simulating the FIFO communication channels with random delays
//  2. multicasting all messages from each node to other nodes
//  3. act as leader(sequencer) in total ordering multicasting

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sstream>
#include <iostream>
#include "Message.h"
#include "MTQueue.h"
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>

#define NODES 4
#define DELAY_CHANNELS 16

int seq = 0;
std::mutex seq_mutex;
int ports[NODES];
int clientfds[NODES];
int max_delay;
MTQueue<Message*>* queues[DELAY_CHANNELS];

//for thread that simulates the FIFO communication channels
//with random delays between any two pairs of nodes
//each channel send out the message when the right time comes
void channel_func(void)
{
    time_t curr_time;
    while(1)
    {
        time(&curr_time);
        for (int i = 0; i < DELAY_CHANNELS; i++) {
            while (!queues[i]->empty() && queues[i]->front()->sent_time == curr_time)
            {
                Message *msg = queues[i]->pop();
                if (send(clientfds[msg->to[0] - 'A'], msg->content, 50, 0) == -1)
                    perror("send");
            }
        }
    }
}

//push messages to FIFO queue
int FIFO(Message* msg)
{
    int queue_number = (msg->from[0] - 'A') * 4 + msg->to[0] - 'A';
    MTQueue<Message*> *mt_queue = queues[queue_number];
    time_t start_time;
    time(&start_time);
    int delay = rand() % max_delay;
    
    if (!mt_queue->empty())                                 // mapping node name msg->content[0] to its queue
    {
        if (mt_queue->back()->sent_time >= start_time + delay)
            msg->sent_time = mt_queue->back()->sent_time;
        else msg->sent_time = start_time + delay;
    }
    else msg->sent_time = start_time + delay;
    mt_queue->push(msg);
    return delay;
}

//for thread that receive messages from every node in the system
void listening_func(char name, int port)
{
    int sockfd;
    char name_[2];
    struct sockaddr_in self;
    name_[0] = name;
    printf("listening to port %d, which is for receiving data from node %c\n", port, name);
    
    //create streaming socket
    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
    {
        perror("socket");
        exit(errno);
    }
    
    //initialize address/port
    bzero(&self, sizeof(self));
    self.sin_family = AF_INET;
    self.sin_port = htons(port);
    self.sin_addr.s_addr = INADDR_ANY;
    
    //assign a port number to the socket
    if ( bind(sockfd, (struct sockaddr *) &self, (socklen_t) sizeof(self)) != 0 )
    {
        perror("socket--bind");
        exit(errno);
    }
    
    //make it a "listening socket"
    if ( listen(sockfd, 20) != 0 )
    {
        perror("socket--listen");
        exit(errno);
    }
    
    int clientfd, numbytes;
    struct sockaddr_in client_addr;
    int addrlen = sizeof(client_addr);
    
    //accept a connection
    clientfd = accept(sockfd, (struct sockaddr *) &client_addr, (socklen_t *) &addrlen);
    clientfds[name - 'A'] = clientfd;
    printf("%s:%d connected port %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), port);
    
    int rand_delay;
    char buf[50];
    while(1)
    {
        for (int i = 0; i < 50; i++) {
            buf[i] = '\0';
        }
        if ((numbytes = recv(clientfd, buf, sizeof(buf), 0)) == -1)
            perror("recv");
        
        std::string buff = buf;
        std::istringstream iss(buff);
        std::string token;
        iss >> token;
        if (token.compare("send") == 0)                     //received buf:(B)"send hello A" -> message format "0_B_hello" (from B)
        {
            Message *msg = new Message();
            strcpy(msg->content, "0_");
            strncat(msg->content, name_, 1);
            strcat(msg->content, "_");
            char tmp[5];
            iss >> tmp;
            strcat(msg->content, tmp);
            iss >> msg->to;
            msg->from[0] = name;
            rand_delay = FIFO(msg);
            printf("random channel delay from %c to %c is %d\n", name, msg->to[0], rand_delay);
            
        } else if(token.compare("6") == 0){
            printf("%c multicasts messages with no sequence number\n", name);
            char tseq[5];
            iss >> tseq;
            char wtmp[5];
            iss >> wtmp;
            char tkey[5];
            iss >> tkey;
            char tmodel[5];
            iss >> tmodel;
            for (int i = 0; i < 4; i++) {
                Message *msg = new Message();
                char cmsg[30];
                sprintf(cmsg, "6 %c %d %s %d %d", name, atoi(tseq), wtmp, atoi(tkey), atoi(tmodel));
                strcpy(msg->content, cmsg);
                strcpy(msg->from, name_);
                msg->to[0] = char('A' + i);
                rand_delay = FIFO(msg);
                printf("random channel delay from %c to %c is %d\n", name, msg->to[0], rand_delay);
                
            }
            
        } else if(token.compare("7") == 0){
            printf("%c multicasts messages with no sequence number\n", name);
            Message *msg = new Message();
            iss >> msg->to;
            char tseq[5];
            iss >> tseq;
            char wtmp[5];
            iss >> wtmp;
            char tkey[5];
            iss >> tkey;
            char tmodel[5];
            iss >> tmodel;
            char ttme[15];
            iss >> ttme;
            char cmsg[30];
            sprintf(cmsg, "7 %c %d %s %d %d %ld", name, atoi(tseq), wtmp, atoi(tkey), atoi(tmodel), atol(ttme));
            strcpy(msg->content, cmsg);
            strcpy(msg->from, name_);
            rand_delay = FIFO(msg);
            printf("random channel delay from %c to %c is %d\n", name, msg->to[0], rand_delay);
                
            
        } else if (token.compare("insert") == 0 || token.compare("update") == 0){
            
            //for updata, insert and get operations
            seq_mutex.lock();                               //received buf: "operation" to format "2_sender_seq#_operation_key(_value)"
            seq++;                                          //critical section
            seq_mutex.unlock();
            std::string mdl;
            iss >> mdl;
            iss >> mdl;
            iss >> mdl;
            
            printf("%c multicasts messages with the sequence number %d\n", name, seq);
            for (int i = 0; i < 4; i++) {
                Message *msg = new Message();
                char cmsg[30];
                sprintf(cmsg, "2 %c %d %s", name, seq, buf);
                strcpy(msg->content, cmsg);
                strcpy(msg->from, name_);
                msg->to[0] = char('A' + i);
                rand_delay = FIFO(msg);
                printf("random channel delay from %c to %c is %d\n", name, msg->to[0], rand_delay);
            }
        } else if (token.compare("get") == 0 ){
            
            //for updata, insert and get operations
            seq_mutex.lock();                               //received buf: "operation" to format "2_sender_seq#_operation_key(_value)"
            seq++;                                          //critical section
            seq_mutex.unlock();
            std::string mdl;
            iss >> mdl;
            iss >> mdl;
            
            printf("%c multicasts messages with the sequence number %d\n", name, seq);
            for (int i = 0; i < 4; i++) {
                Message *msg = new Message();
                char cmsg[30];
                sprintf(cmsg, "2 %c %d %s", name, seq, buf);
                strcpy(msg->content, cmsg);
                strcpy(msg->from, name_);
                msg->to[0] = char('A' + i);
                rand_delay = FIFO(msg);
                printf("random channel delay from %c to %c is %d\n", name, msg->to[0], rand_delay);
            }
        } else if (token.compare("delete") == 0 ){
            printf("%c multicasts messages with the sequence number %d\n", name, seq);
            std::string mdl;
            iss >> mdl;
            for (int i = 0; i < 4; i++) {
                Message *msg = new Message();
                char cmsg[30];
                sprintf(cmsg, "3 delete %d", atoi(mdl.c_str()));
                strcpy(msg->content, cmsg);
                strcpy(msg->from, name_);
                msg->to[0] = char('A' + i);
                rand_delay = FIFO(msg);
                printf("random channel delay from %c to %c is %d\n", name, msg->to[0], rand_delay);
            }
        }
        
        if(buf[0] == '\n')
        {
            shutdown(sockfd, 2);
            //close(sockfd);
        }
    }
}

//read configuration file to get ports and max delay for channels
void read_config(int ports[], int &max_delay)
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
    else printf("unable to open file");
}

//main thread for initialing four threads for four nodes
//each thread is listening to each according port

int main(int argc, char * argv[])
{
    read_config(ports, max_delay);
    for (int i = 0; i < DELAY_CHANNELS; i++)
        queues[i] = new MTQueue<Message*>();
    
    std::thread thread_A(listening_func, 'A', ports[0]);
    std::thread thread_B(listening_func, 'B', ports[1]);
    std::thread thread_C(listening_func, 'C', ports[2]);
    std::thread thread_D(listening_func, 'D', ports[3]);
    std::thread thread_channel(channel_func);
    
    thread_A.join();
    thread_B.join();
    thread_C.join();
    thread_D.join();
    thread_channel.join();
    return 0;
}

