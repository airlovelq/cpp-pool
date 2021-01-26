#ifndef _CONNECTION_H
#define _CONNECTION_H
#include <iostream>
#include <memory>
#include "ConnectionPool.h"

class CMockConnection {
public:
    CMockConnection(int i) {
        std::cout<<"build connection"<<std::endl;
    };

    ~CMockConnection() {
        std::cout<<"destroy connection"<<std::endl;
    };

public:
    void close(){
        _isClosed = true;
        std::cout<<"close"<<std::endl;
    }

    bool _isClosed = false;
};

#endif
