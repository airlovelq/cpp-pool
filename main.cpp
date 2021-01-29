#include <iostream>
#include <thread>
#include "ObjectPool.h"
#include "Connection.h"

using namespace std;

int i = 0;
void set() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    for ( int j = 0 ; j<100000; j++ )
        i++;
}

void getnum() {
    for ( int j = 0 ; j<100000 ; j++ )
        printf("%d\n", i);
}

struct B {
    int i;
};

class A {
    A(B&& k){}
};

int main()
{
//    auto sthread = std::thread(set);
//    auto gthread = std::thread(getnum);
//    sthread.join();
//    gthread.join();
    MockArg arg{909};
    auto op = ObjectPool<CMockObject,MockArg>::createInstance(4, arg);
    cout<<op->getPoolSize()<<endl;
    auto con = op->getObject();
//    auto con1 = op->getConnection();
//    auto con2 = op->getConnection();
//    auto con3 = op->getConnection();
//    auto con4 = op.getConnection();
//    op.getConnection();
//    op.getConnection();
//    op.getConnection();
//    op.getConnection();
//    op.getConnection();
//    op.getConnection();
//    op.getConnection();
//    op.getConnection();
//    op.getConnection();
//    op.getConnection();
    cout<<op->getPoolSize()<<endl;
//    op.retConnect(con);
    cout<<op->getPoolSize()<<endl;

    op->destroy();
    op.reset();
//    op->resetPoolSize(2);
//    con2.reset();
//    con3.reset();
    con.reset();
//    std::move(con2);
//    std::move(con3);
    cout << "Hello World!" << endl;
    return 0;
}
