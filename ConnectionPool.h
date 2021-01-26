#ifndef _CONNECTIONPOOL_H
#define _CONNECTIONPOOL_H
#include <memory>
#include <list>
#include <mutex>
#include <atomic>
#include <functional>

template<typename T>
struct IsAutoClose{
    template<typename U, void (U::*)()> struct HELPS;
    template<typename U> static char Test(HELPS<U, &U::close>*);
    template<typename U> static int Test(...);
    const static bool value = sizeof(Test<T>(0)) != sizeof(char);
};

template <typename CONNECTION, typename CONNECTION_ARGS>
class ConnectionPool {

    using RecycleFunc = std::function<void(CONNECTION*)>;

    using ConnectionAutoRecycle = std::unique_ptr<CONNECTION, std::function<void(CONNECTION*)>>;

    using ConnectionPoolPtr = std::shared_ptr<ConnectionPool<CONNECTION, CONNECTION_ARGS>>;

public:

    static ConnectionPoolPtr getInstance(int pool_max_size, CONNECTION_ARGS args);

protected:
    ConnectionPool(int pool_max_size, CONNECTION_ARGS args);

public:
    ~ConnectionPool();

public:
    ConnectionAutoRecycle getConnection();

    static void retConnect(CONNECTION* con);

    int getPoolSize();

    void initConnectPool(int initial_size);

    void destroyPool();

    void resetPoolSize(int size);

    void destroy() {
        _sg_instance.reset();
    }

private:
    template<typename T = CONNECTION>
    typename std::enable_if<IsAutoClose<T>::value>::type destroyOneConnection();

    template<typename T = CONNECTION>
    typename std::enable_if<!IsAutoClose<T>::value>::type destroyOneConnection();

    template<typename T = CONNECTION>
    static typename std::enable_if<IsAutoClose<T>::value>::type destroyOneConnection(CONNECTION* con);

    template<typename T = CONNECTION>
    static typename std::enable_if<!IsAutoClose<T>::value>::type destroyOneConnection(CONNECTION* con);

private:
    static ConnectionPoolPtr _sg_instance;

    static std::mutex _sg_lock;

    std::list<std::unique_ptr<CONNECTION>> _list_con;

    std::mutex _lock;

    CONNECTION_ARGS args;

    int _pool_max_size;      // max size

    int _pool_size;          // cur size
};

template <typename CONNECTION, typename CONNECTION_ARGS>
std::mutex ConnectionPool<CONNECTION, CONNECTION_ARGS>::_sg_lock;

template <typename CONNECTION, typename CONNECTION_ARGS>
typename ConnectionPool<CONNECTION, CONNECTION_ARGS>::ConnectionPoolPtr ConnectionPool<CONNECTION, CONNECTION_ARGS>::_sg_instance(nullptr);

template <typename CONNECTION, typename CONNECTION_ARGS>
ConnectionPool<CONNECTION, CONNECTION_ARGS>::ConnectionPool(int pool_max_size, CONNECTION_ARGS args)
    :_pool_max_size(pool_max_size), args(args){
    initConnectPool(pool_max_size/2);
}

template <typename CONNECTION, typename CONNECTION_ARGS>
ConnectionPool<CONNECTION, CONNECTION_ARGS>::~ConnectionPool() {
    destroyPool();
}

template <typename CONNECTION, typename CONNECTION_ARGS>
typename ConnectionPool<CONNECTION, CONNECTION_ARGS>::ConnectionPoolPtr ConnectionPool<CONNECTION, CONNECTION_ARGS>::getInstance(int pool_max_size, CONNECTION_ARGS args) {
    std::lock_guard<std::mutex> lg(_sg_lock);
    if ( !_sg_instance ) {
        _sg_instance = std::shared_ptr<ConnectionPool<CONNECTION, CONNECTION_ARGS>>(new ConnectionPool<CONNECTION, CONNECTION_ARGS>(pool_max_size, args));
    }
    return _sg_instance;
}

template <typename CONNECTION, typename CONNECTION_ARGS>
void ConnectionPool<CONNECTION, CONNECTION_ARGS>::initConnectPool(int initial_size) {
    std::lock_guard<std::mutex> lg(_lock);
    for ( int i = 0 ; i < initial_size ; ++i ) {
        _list_con.push_back(std::make_unique<CONNECTION>(args));
    }
    _pool_size = initial_size;
}

template <typename CONNECTION, typename CONNECTION_ARGS>
void ConnectionPool<CONNECTION, CONNECTION_ARGS>::destroyPool() {
    std::lock_guard<std::mutex> lg(_lock);
    while ( _list_con.size() > 0 ) {
        destroyOneConnection();
    }
}

template <typename CONNECTION, typename CONNECTION_ARGS>
template<typename T>
typename std::enable_if<!IsAutoClose<T>::value>::type ConnectionPool<CONNECTION, CONNECTION_ARGS>::destroyOneConnection() {
    auto &con = _list_con.front();
    con->close();
    _list_con.pop_front();
}

template <typename CONNECTION, typename CONNECTION_ARGS>
template<typename T>
typename std::enable_if<IsAutoClose<T>::value>::type ConnectionPool<CONNECTION, CONNECTION_ARGS>::destroyOneConnection() {
    _list_con.pop_front();
}

template <typename CONNECTION, typename CONNECTION_ARGS>
template<typename T>
typename std::enable_if<IsAutoClose<T>::value>::type ConnectionPool<CONNECTION, CONNECTION_ARGS>::destroyOneConnection(CONNECTION* con) {
    delete con;
    con = nullptr;
}

template <typename CONNECTION, typename CONNECTION_ARGS>
template<typename T>
typename std::enable_if<!IsAutoClose<T>::value>::type ConnectionPool<CONNECTION, CONNECTION_ARGS>::destroyOneConnection(CONNECTION* con) {
    con->close();
    delete con;
    con = nullptr;
}

template <typename CONNECTION, typename CONNECTION_ARGS>
int ConnectionPool<CONNECTION, CONNECTION_ARGS>::getPoolSize() {
    std::lock_guard<std::mutex> lg(this->_lock);
    return _pool_size;
}

template <typename CONNECTION, typename CONNECTION_ARGS>
typename ConnectionPool<CONNECTION, CONNECTION_ARGS>::ConnectionAutoRecycle ConnectionPool<CONNECTION, CONNECTION_ARGS>::getConnection() {
    while ( true ) {
        std::lock_guard<std::mutex> lg(_lock);
        if ( _list_con.size() > 0 ) {
            auto &con = _list_con.front();

            auto ret = std::unique_ptr<CONNECTION, RecycleFunc>(con.release(), std::bind(&ConnectionPool<CONNECTION, CONNECTION_ARGS>::retConnect, std::placeholders::_1));
            _list_con.pop_front();
            return std::move(ret);
        } else {
            if ( _pool_size < _pool_max_size ) {
                ++_pool_size;
                return std::unique_ptr<CONNECTION, RecycleFunc>(new CONNECTION(args), std::bind(&ConnectionPool<CONNECTION, CONNECTION_ARGS>::retConnect, std::placeholders::_1));
            }
        }
    }
}

template <typename CONNECTION, typename CONNECTION_ARGS>
void ConnectionPool<CONNECTION, CONNECTION_ARGS>::retConnect(CONNECTION* con) {
    std::unique_lock<std::mutex> ulg(_sg_lock);
    if ( !_sg_instance ) {
        destroyOneConnection(con);
        return;
    }
    ulg.unlock();
    std::lock_guard<std::mutex> lg(_sg_instance->_lock);

    if ( _sg_instance->_list_con.size() < _sg_instance->_pool_size ) {
        _sg_instance->_list_con.push_back(std::unique_ptr<CONNECTION>(con));
    } else if ( _sg_instance->_list_con.size() < _sg_instance->_pool_max_size ) {
        _sg_instance->_list_con.push_back(std::unique_ptr<CONNECTION>(con));
        _sg_instance->_pool_size++;
    } else {
        destroyOneConnection(con);
    }
}

template <typename CONNECTION, typename CONNECTION_ARGS>
void ConnectionPool<CONNECTION, CONNECTION_ARGS>::resetPoolSize(int size) {
    std::lock_guard<std::mutex> lg(_lock);
    if ( size > _pool_max_size ) {
        _pool_max_size = size;
    } else if ( size < _pool_max_size ) {
        _pool_max_size = size;
        if ( _pool_size > size ) {
            _pool_size = size;
        }
        while ( _list_con.size() > size ) {
            destroyOneConnection();
        }
    }
}

#endif
