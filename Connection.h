#ifndef _CONNECTION_H
#define _CONNECTION_H
#include <iostream>
#include <memory>
#include "ObjectPool.h"
class CConnectionBase {
protected:
    CConnectionBase() {}

public:
    virtual ~CConnectionBase() {}

public:
    virtual void connect() = 0;
    virtual void close() = 0;
};

class CMockConnection : public CConnectionBase {
public:
    CMockConnection() {
        std::cout<<"build connection"<<std::endl;
    };

    ~CMockConnection() {
        std::cout<<"destroy connection"<<std::endl;
    };

public:
    void close() {
        _isClosed = true;
        std::cout<<"close"<<std::endl;
    }

    void connect() {
        std::cout<<"connect"<<std::endl;
    }

    bool _isClosed = false;
};

struct MockArg {
//    MockArg(const MockArg& ) = delete;
    int i;
};

class CMockObject {
public:
    CMockObject() = delete;
    CMockObject(const MockArg& i) {
        std::cout<<"build object"<<std::endl;
    }

    ~CMockObject() {
        std::cout<<"destroy object"<<std::endl;
    }
};

#endif
