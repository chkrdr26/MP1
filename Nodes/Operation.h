//
//  Operation.h
//  Nodes
//
//  operations like "insert", "update", "get" and so on
//  once an operation is received, according to its seq_num
//  the middleware decides whether to push it to the buffer
//  or to deliver it to the key-value store application layer

class Operation {
public:
    int seq_num;
    char oper[10];
    int key;
    int value;
    int model;
    char from[2];
    time_t lastWrite;
    Operation() {}
};