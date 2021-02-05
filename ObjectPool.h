#ifndef _CONNECTIONPOOL_H
#define _CONNECTIONPOOL_H
#include <memory>
#include <list>
#include <mutex>
#include <atomic>
#include <functional>
#include <thread>
#include <iostream>


template<typename T>
struct IsConnection{
    template<typename U, void (U::*)(), void (U::*)()> struct HELPS;
    template<typename U> static char Test(HELPS<U, &U::close, &U::connect>*);
    template<typename U> static int Test(...);
    const static bool value = sizeof(Test<T>(0)) == sizeof(char);
};


template <typename OBJECT, typename OBJECT_ARGS = void>
class ObjectPool {

    static_assert (std::is_void<OBJECT_ARGS>::value && std::is_constructible<OBJECT>::value || std::is_constructible<OBJECT, OBJECT_ARGS>::value, "object arg not match" );
//    static_assert (!std::is_void<OBJECT_ARGS>::value && std::is_copy_constructible<OBJECT_ARGS>::value, "object arg not copyable" );
public:
    using ObjectRecycleFunc = std::function<void(OBJECT*)>;

    using ObjectAutoRecycle = std::unique_ptr<OBJECT, std::function<void(OBJECT*)>>;

    using ObjectPoolPtr = std::shared_ptr<ObjectPool<OBJECT, OBJECT_ARGS>>;

public:
//    static ObjectPoolPtr createInstance(int pool_max_size);
//    static ObjectPoolPtr createInstance(typename std::enable_if<std::is_void<OBJECT_ARGS>::value, int>::type pool_max_size);

    template<typename T = OBJECT_ARGS>
    static ObjectPoolPtr createInstance(int pool_max_size, typename std::enable_if<std::is_void<T>::value, std::nullptr_t>::type args=nullptr);

    template<typename T = OBJECT_ARGS>
    static ObjectPoolPtr createInstance(int pool_max_size, typename std::enable_if<(!std::is_class<T>::value) && (!std::is_void<T>::value), T>::type args);

    template<typename T = OBJECT_ARGS>
    static ObjectPoolPtr createInstance(int pool_max_size, typename std::enable_if<std::is_class<T>::value, const T&>::type args);

    static ObjectPoolPtr getInstance();

protected:
    template<typename T = OBJECT_ARGS>
    ObjectPool(int pool_max_size, typename std::enable_if<std::is_void<T>::value, std::nullptr_t>::type args);

    template<typename T = OBJECT_ARGS>
    ObjectPool(int pool_max_size, typename std::enable_if<(!std::is_class<T>::value) && (!std::is_void<T>::value), T>::type args);

    template<typename T = OBJECT_ARGS>
    ObjectPool(int pool_max_size, typename std::enable_if<std::is_class<T>::value, const T&>::type args);

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool(ObjectPool&&) = delete;

    ObjectPool& operator=(const ObjectPool&) = delete;
    ObjectPool& operator=(ObjectPool&&) = delete;

public:
    ~ObjectPool();

public:
    template<typename T=OBJECT, typename U=OBJECT_ARGS>
    typename std::enable_if<!IsConnection<T>::value && std::is_void<U>::value, ObjectAutoRecycle>::type getObject();

    template<typename T=OBJECT, typename U=OBJECT_ARGS>
    typename std::enable_if<IsConnection<T>::value && std::is_void<U>::value, ObjectAutoRecycle>::type getObject();

    template<typename T=OBJECT, typename U=OBJECT_ARGS>
    typename std::enable_if<!IsConnection<T>::value && !std::is_void<U>::value, ObjectAutoRecycle>::type getObject();

    template<typename T=OBJECT, typename U=OBJECT_ARGS>
    typename std::enable_if<IsConnection<T>::value && !std::is_void<U>::value, ObjectAutoRecycle>::type getObject();

    static void retObject(OBJECT* obj);

    int getPoolSize();

    int getPoolMaxSize();

    template<typename T=OBJECT, typename U=OBJECT_ARGS>
    typename std::enable_if<IsConnection<T>::value && std::is_void<U>::value>::type initPool(int initial_size);

    template<typename T=OBJECT, typename U=OBJECT_ARGS>
    typename std::enable_if<IsConnection<T>::value && !std::is_void<U>::value>::type initPool(int initial_size);

    template<typename T=OBJECT, typename U=OBJECT_ARGS>
    typename std::enable_if<!IsConnection<T>::value && std::is_void<U>::value>::type initPool(int initial_size);

    template<typename T=OBJECT, typename U=OBJECT_ARGS>
    typename std::enable_if<!IsConnection<T>::value && !std::is_void<U>::value>::type initPool(int initial_size);


    void destroyPool();

    void resetPoolMaxSize(int size);


private:
    template<typename T = OBJECT>
    typename std::enable_if<IsConnection<T>::value>::type destroyOneObject();

    template<typename T = OBJECT>
    typename std::enable_if<!IsConnection<T>::value>::type destroyOneObject();

    template<typename T = OBJECT>
    static typename std::enable_if<IsConnection<T>::value>::type destroyOneObject(OBJECT* obj);

    template<typename T = OBJECT>
    static typename std::enable_if<!IsConnection<T>::value>::type destroyOneObject(OBJECT* obj);

private:
    static ObjectPoolPtr _sg_instance;

    static std::mutex _sg_lock;

    std::list<std::unique_ptr<OBJECT>> _list_con;

    std::mutex _lock;

    typename std::conditional<std::is_void<OBJECT_ARGS>::value, std::nullptr_t, OBJECT_ARGS>::type args;

    int _pool_max_size;      // max size

    int _pool_size;          // cur size

private:
    std::mutex _idle_lock;

    std::thread _idle_thread;

    int _max_idle_seconds = 30;

    time_t _last_op_time;

    bool _is_destroyed = false;

public:
    void beginAutoScaleThread();

private:
    void autoScale();

    void updateLastOperationTime();
};

template <typename OBJECT, typename OBJECT_ARGS>
std::mutex ObjectPool<OBJECT, OBJECT_ARGS>::_sg_lock;

template <typename OBJECT, typename OBJECT_ARGS>
typename ObjectPool<OBJECT, OBJECT_ARGS>::ObjectPoolPtr ObjectPool<OBJECT, OBJECT_ARGS>::_sg_instance(nullptr);

//template <typename OBJECT, typename OBJECT_ARGS>
//template<typename std::enable_if<std::is_void<OBJECT_ARGS>::value, decltype (nullptr)>::type>
//ObjectPool<OBJECT, OBJECT_ARGS>::ObjectPool(int pool_max_size)
//    :_pool_max_size(pool_max_size){
//    initPool(pool_max_size/2);
//}

template <typename OBJECT, typename OBJECT_ARGS>
template<typename T>
ObjectPool<OBJECT, OBJECT_ARGS>::ObjectPool(int pool_max_size, typename std::enable_if<std::is_void<T>::value, std::nullptr_t>::type args)
    :_pool_max_size(pool_max_size), args(args) {
    initPool(pool_max_size/2);
}

template <typename OBJECT, typename OBJECT_ARGS>
template<typename T>
ObjectPool<OBJECT, OBJECT_ARGS>::ObjectPool(int pool_max_size, typename std::enable_if<(!std::is_class<T>::value) && (!std::is_void<T>::value), T>::type args)
    :_pool_max_size(pool_max_size), args(args){
    initPool(pool_max_size/2);
}

template <typename OBJECT, typename OBJECT_ARGS>
template<typename T>
ObjectPool<OBJECT, OBJECT_ARGS>::ObjectPool(int pool_max_size, typename std::enable_if<std::is_class<T>::value, const T&>::type args)
    :_pool_max_size(pool_max_size), args(args){
    initPool(pool_max_size/2);
}

template <typename OBJECT, typename OBJECT_ARGS>
ObjectPool<OBJECT, OBJECT_ARGS>::~ObjectPool() {
    destroyPool();
    _is_destroyed = true;
    _idle_thread.join();
}

//template <typename OBJECT, typename OBJECT_ARGS>
//typename ObjectPool<OBJECT, OBJECT_ARGS>::ObjectPoolPtr ObjectPool<OBJECT, OBJECT_ARGS>::createInstance(int pool_max_size) {
//    std::lock_guard<std::mutex> lg(_sg_lock);
//    if ( !_sg_instance ) {
//        _sg_instance = std::shared_ptr<ObjectPool<OBJECT, OBJECT_ARGS>>(new ObjectPool<OBJECT, OBJECT_ARGS>(pool_max_size, nullptr));
//        _sg_instance->autoScale();
//    }
//    return _sg_instance;
//}


template <typename OBJECT, typename OBJECT_ARGS>
template<typename T>
typename ObjectPool<OBJECT, OBJECT_ARGS>::ObjectPoolPtr ObjectPool<OBJECT, OBJECT_ARGS>::createInstance(int pool_max_size, typename std::enable_if<std::is_void<T>::value, std::nullptr_t>::type args) {
    std::lock_guard<std::mutex> lg(_sg_lock);
    if ( !_sg_instance ) {
        _sg_instance = std::shared_ptr<ObjectPool<OBJECT, OBJECT_ARGS>>(new ObjectPool<OBJECT, OBJECT_ARGS>(pool_max_size,args));
        _sg_instance->autoScale();
    }
    return _sg_instance;
}

template <typename OBJECT, typename OBJECT_ARGS>
template<typename T>
typename ObjectPool<OBJECT, OBJECT_ARGS>::ObjectPoolPtr ObjectPool<OBJECT, OBJECT_ARGS>::createInstance(int pool_max_size, typename std::enable_if<(!std::is_class<T>::value) && (!std::is_void<T>::value), T>::type args) {
    std::lock_guard<std::mutex> lg(_sg_lock);
    if ( !_sg_instance ) {
        _sg_instance = std::shared_ptr<ObjectPool<OBJECT, OBJECT_ARGS>>(new ObjectPool<OBJECT, OBJECT_ARGS>(pool_max_size, args));
        _sg_instance->autoScale();
    }
    return _sg_instance;
}

template <typename OBJECT, typename OBJECT_ARGS>
template<typename T>
typename ObjectPool<OBJECT, OBJECT_ARGS>::ObjectPoolPtr ObjectPool<OBJECT, OBJECT_ARGS>::createInstance(int pool_max_size, typename std::enable_if<std::is_class<T>::value, const T&>::type args) {
    std::lock_guard<std::mutex> lg(_sg_lock);
    if ( !_sg_instance ) {
        _sg_instance = std::shared_ptr<ObjectPool<OBJECT, OBJECT_ARGS>>(new ObjectPool<OBJECT, OBJECT_ARGS>(pool_max_size, args));
    }
    return _sg_instance;
}

template <typename OBJECT, typename OBJECT_ARGS>
typename ObjectPool<OBJECT, OBJECT_ARGS>::ObjectPoolPtr ObjectPool<OBJECT, OBJECT_ARGS>::getInstance() {
    return _sg_instance;
}

template <typename OBJECT, typename OBJECT_ARGS>
template<typename T, typename U>
typename std::enable_if<!IsConnection<T>::value && !std::is_void<U>::value>::type ObjectPool<OBJECT, OBJECT_ARGS>::initPool(int initial_size) {
    updateLastOperationTime();
    std::lock_guard<std::mutex> lg(_lock);
    for ( int i = 0 ; i < initial_size ; ++i ) {
        _list_con.push_back(std::make_unique<OBJECT>(args));
    }
    _pool_size = initial_size;
}

template <typename OBJECT, typename OBJECT_ARGS>
template<typename T, typename U>
typename std::enable_if<IsConnection<T>::value && !std::is_void<U>::value>::type ObjectPool<OBJECT, OBJECT_ARGS>::initPool(int initial_size) {
    updateLastOperationTime();
    std::lock_guard<std::mutex> lg(_lock);
    for ( int i = 0 ; i < initial_size ; ++i ) {
        _list_con.push_back(std::make_unique<OBJECT>(args));
        _list_con.back()->connect();
    }
    _pool_size = initial_size;
}

template <typename OBJECT, typename OBJECT_ARGS>
template<typename T, typename U>
typename std::enable_if<!IsConnection<T>::value && std::is_void<U>::value>::type ObjectPool<OBJECT, OBJECT_ARGS>::initPool(int initial_size) {
    updateLastOperationTime();
    std::lock_guard<std::mutex> lg(_lock);
    for ( int i = 0 ; i < initial_size ; ++i ) {
        _list_con.push_back(std::make_unique<OBJECT>());
    }
    _pool_size = initial_size;
}

template <typename OBJECT, typename OBJECT_ARGS>
template<typename T, typename U>
typename std::enable_if<IsConnection<T>::value && std::is_void<U>::value>::type ObjectPool<OBJECT, OBJECT_ARGS>::initPool(int initial_size) {
    updateLastOperationTime();
    std::lock_guard<std::mutex> lg(_lock);
    for ( int i = 0 ; i < initial_size ; ++i ) {
        _list_con.push_back(std::make_unique<OBJECT>());
        _list_con.back()->connect();
    }
    _pool_size = initial_size;
}

template <typename OBJECT, typename OBJECT_ARGS>
void ObjectPool<OBJECT, OBJECT_ARGS>::destroyPool() {
    std::lock_guard<std::mutex> lg(_lock);
    while ( _list_con.size() > 0 ) {
        destroyOneObject();
    }
}

template <typename OBJECT, typename OBJECT_ARGS>
template<typename T>
typename std::enable_if<IsConnection<T>::value>::type ObjectPool<OBJECT, OBJECT_ARGS>::destroyOneObject() {
    auto &obj = _list_con.front();
    obj->close();
    _list_con.pop_front();
}

template <typename OBJECT, typename OBJECT_ARGS>
template<typename T>
typename std::enable_if<!IsConnection<T>::value>::type ObjectPool<OBJECT, OBJECT_ARGS>::destroyOneObject() {
    _list_con.pop_front();
    std::cout<<"destroy success"<<std::endl;
}

template <typename OBJECT, typename OBJECT_ARGS>
template<typename T>
typename std::enable_if<!IsConnection<T>::value>::type ObjectPool<OBJECT, OBJECT_ARGS>::destroyOneObject(OBJECT* obj) {
    delete obj;
    obj = nullptr;
}

template <typename OBJECT, typename OBJECT_ARGS>
template<typename T>
typename std::enable_if<IsConnection<T>::value>::type ObjectPool<OBJECT, OBJECT_ARGS>::destroyOneObject(OBJECT* obj) {
    obj->close();
    delete obj;
    obj = nullptr;
}

template <typename OBJECT, typename OBJECT_ARGS>
int ObjectPool<OBJECT, OBJECT_ARGS>::getPoolSize() {
    std::lock_guard<std::mutex> lg(this->_lock);
    return _pool_size;
}

template <typename OBJECT, typename OBJECT_ARGS>
int ObjectPool<OBJECT, OBJECT_ARGS>::getPoolMaxSize() {
    std::lock_guard<std::mutex> lg(this->_lock);
    return _pool_max_size;
}

template <typename OBJECT, typename OBJECT_ARGS>
template<typename T, typename U>
typename std::enable_if<!IsConnection<T>::value && !std::is_void<U>::value, typename ObjectPool<OBJECT, OBJECT_ARGS>::ObjectAutoRecycle>::type ObjectPool<OBJECT, OBJECT_ARGS>::getObject() {
    updateLastOperationTime();
    while ( true ) {
        std::lock_guard<std::mutex> lg(_lock);
        if ( _list_con.size() > 0 ) {
            auto &obj = _list_con.front();

            auto ret = std::unique_ptr<OBJECT, ObjectRecycleFunc>(obj.release(), std::bind(&ObjectPool<OBJECT, OBJECT_ARGS>::retObject, std::placeholders::_1));
            _list_con.pop_front();
            return std::move(ret);
        } else {
            if ( _pool_size < _pool_max_size ) {
                ++_pool_size;
                return std::unique_ptr<OBJECT, ObjectRecycleFunc>(new OBJECT(args), std::bind(&ObjectPool<OBJECT, OBJECT_ARGS>::retObject, std::placeholders::_1));
            }
        }
    }
}

template <typename OBJECT, typename OBJECT_ARGS>
template<typename T, typename U>
typename std::enable_if<IsConnection<T>::value && !std::is_void<U>::value, typename ObjectPool<OBJECT, OBJECT_ARGS>::ObjectAutoRecycle>::type ObjectPool<OBJECT, OBJECT_ARGS>::getObject() {
    updateLastOperationTime();
    while ( true ) {
        std::lock_guard<std::mutex> lg(_lock);
        if ( _list_con.size() > 0 ) {
            auto &obj = _list_con.front();

            auto ret = std::unique_ptr<OBJECT, ObjectRecycleFunc>(obj.release(), std::bind(&ObjectPool<OBJECT, OBJECT_ARGS>::retObject, std::placeholders::_1));
            _list_con.pop_front();
            return std::move(ret);
        } else {
            if ( _pool_size < _pool_max_size ) {
                ++_pool_size;
                auto obj = new OBJECT(args);
                obj->connect();
                return std::unique_ptr<OBJECT, ObjectRecycleFunc>(obj, std::bind(&ObjectPool<OBJECT, OBJECT_ARGS>::retObject, std::placeholders::_1));
            }
        }
    }
}

template <typename OBJECT, typename OBJECT_ARGS>
template<typename T, typename U>
typename std::enable_if<!IsConnection<T>::value && std::is_void<U>::value, typename ObjectPool<OBJECT, OBJECT_ARGS>::ObjectAutoRecycle>::type ObjectPool<OBJECT, OBJECT_ARGS>::getObject() {
    updateLastOperationTime();
    while ( true ) {
        std::lock_guard<std::mutex> lg(_lock);
        if ( _list_con.size() > 0 ) {
            auto &obj = _list_con.front();

            auto ret = std::unique_ptr<OBJECT, ObjectRecycleFunc>(obj.release(), std::bind(&ObjectPool<OBJECT, OBJECT_ARGS>::retObject, std::placeholders::_1));
            _list_con.pop_front();
            return std::move(ret);
        } else {
            if ( _pool_size < _pool_max_size ) {
                ++_pool_size;
                return std::unique_ptr<OBJECT, ObjectRecycleFunc>(new OBJECT(), std::bind(&ObjectPool<OBJECT, OBJECT_ARGS>::retObject, std::placeholders::_1));
            }
        }
    }
}

template <typename OBJECT, typename OBJECT_ARGS>
template<typename T, typename U>
typename std::enable_if<IsConnection<T>::value && std::is_void<U>::value, typename ObjectPool<OBJECT, OBJECT_ARGS>::ObjectAutoRecycle>::type ObjectPool<OBJECT, OBJECT_ARGS>::getObject() {
    updateLastOperationTime();
    while ( true ) {
        std::lock_guard<std::mutex> lg(_lock);
        if ( _list_con.size() > 0 ) {
            auto &obj = _list_con.front();

            auto ret = std::unique_ptr<OBJECT, ObjectRecycleFunc>(obj.release(), std::bind(&ObjectPool<OBJECT, OBJECT_ARGS>::retObject, std::placeholders::_1));
            _list_con.pop_front();
            return std::move(ret);
        } else {
            if ( _pool_size < _pool_max_size ) {
                ++_pool_size;
                auto obj = new OBJECT();
                obj->connect();
                return std::unique_ptr<OBJECT, ObjectRecycleFunc>(obj, std::bind(&ObjectPool<OBJECT, OBJECT_ARGS>::retObject, std::placeholders::_1));
            }
        }
    }
}

template <typename OBJECT, typename OBJECT_ARGS>
void ObjectPool<OBJECT, OBJECT_ARGS>::retObject(OBJECT* obj) {
    std::unique_lock<std::mutex> ulg(_sg_lock);
    if ( !_sg_instance ) {
        destroyOneObject(obj);
        return;
    }
    ulg.unlock();
    std::lock_guard<std::mutex> lg(_sg_instance->_lock);

    if ( _sg_instance->_list_con.size() < _sg_instance->_pool_size ) {
        _sg_instance->_list_con.push_back(std::unique_ptr<OBJECT>(obj));
    } else if ( _sg_instance->_list_con.size() < _sg_instance->_pool_max_size ) {
        _sg_instance->_list_con.push_back(std::unique_ptr<OBJECT>(obj));
        _sg_instance->_pool_size++;
    } else {
        destroyOneObject(obj);
    }
}

template <typename OBJECT, typename OBJECT_ARGS>
void ObjectPool<OBJECT, OBJECT_ARGS>::resetPoolMaxSize(int size) {
    std::lock_guard<std::mutex> lg(_lock);
    if ( size > _pool_max_size ) {
        _pool_max_size = size;
    } else if ( size < _pool_max_size ) {
        _pool_max_size = size;
        if ( _pool_size > size ) {
            _pool_size = size;
        }
        while ( _list_con.size() > size ) {
            destroyOneObject();
        }
    }
}

template <typename OBJECT, typename OBJECT_ARGS>
void ObjectPool<OBJECT, OBJECT_ARGS>::beginAutoScaleThread() {
    _idle_thread = std::thread(&ObjectPool<OBJECT, OBJECT_ARGS>::autoScale, this);
}

template <typename OBJECT, typename OBJECT_ARGS>
void ObjectPool<OBJECT, OBJECT_ARGS>::autoScale() {
    time_t t;
    while ( true ) {
        if ( _is_destroyed ) {
            std::cout<<"auto scale terminated"<<std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
        time(&t);
        if ( t-_last_op_time > 10 ) {
            if ( _pool_size > _pool_max_size/2 ) {
                _pool_size = _pool_max_size/2;
                while ( _list_con.size() > _pool_size ) {
                    destroyOneObject();
                }
            }

        }
    }
}

template <typename OBJECT, typename OBJECT_ARGS>
void ObjectPool<OBJECT, OBJECT_ARGS>::updateLastOperationTime() {
    std::lock_guard<std::mutex> lg(_idle_lock);
    time(&_last_op_time);
}
#endif
