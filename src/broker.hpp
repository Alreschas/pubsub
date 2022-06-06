#pragma once

#include <iostream>
#include <map>
#include <deque>
#include <string>
#include <mutex>
#include <functional>
#include <thread>
#include <unistd.h>

#include "topic_func_pair_list.hpp"

namespace pubsub {
class BrokerCore {
public:
    void run() {
        th = std::thread(&BrokerCore::loop, this);
        usleep(100); //スレッドが確実に立ち上がるまで待つ。
    }

    void stop() {
        mtx.lock();
        stop_request = true;
        cond.notify_one();
        mtx.unlock();
        if (th.joinable()) {
            th.join();
        }
    }

    template<class ClassType, class DataTypeWithConstAndReference>
    unsigned int subscribe(const std::string &topic, void (ClassType::*func_ptr)(DataTypeWithConstAndReference), ClassType *caller, size_t max_que_size = 0) {
        std::lock_guard<std::mutex> lk(mtx);
        std::function<void(DataTypeWithConstAndReference)> functional = std::bind(func_ptr, caller, std::placeholders::_1);

        return func_buffer.add(topic, functional, max_que_size);
    }

    template<class DataType>
    bool getLatestData(const std::string &topic, DataType &data) {
        return func_buffer.getLatestData<DataType>(topic, data);
    }

    void close_subscribe(const std::string &topic, unsigned int handler) {
        std::lock_guard<std::mutex> lk(mtx);
        func_buffer.close_func(topic, handler);
    }

    void pause_subscribe(const std::string &topic, unsigned int handler) {
        std::lock_guard<std::mutex> lk(mtx);
        func_buffer.pause_func(topic, handler);
    }

    void resume_subscribe(const std::string &topic, unsigned int handler) {
        std::lock_guard<std::mutex> lk(mtx);
        func_buffer.resume_func(topic, handler);
    }

    void close_subscribe_serialized(unsigned int handler) {
        std::lock_guard<std::mutex> lk(mtx);
        func_buffer.close_func_serialized(handler);
    }

    template<class DataType>
    void publish(const std::string &topic, const DataType &value) {
        std::lock_guard<std::mutex> lk(mtx);

        func_buffer.add_data(topic, value);

        cond.notify_one();
    }

    void publish_msg(const std::string &topic, const std::string &msg,int sender_id) {
        std::lock_guard<std::mutex> lk(mtx);

        func_buffer.add_msg(topic, msg,sender_id);

        cond.notify_one();
    }

    template<class ClassType>
    int addFunc(void(ClassType::*func_ptr)(const std::string&,const std::string&), ClassType *caller, size_t max_queue_size = 0,int sender_id = -1) {
        auto functional = std::bind(func_ptr, caller, std::placeholders::_1, std::placeholders::_2);
        return func_buffer.addFunc(functional,sender_id);
    }

    template<class DataType, class SerializerType>
    void setSerializer(const std::string &topic) {
        func_buffer.setSerializer<DataType, SerializerType>(topic);
    }


private:

    void loop() {

        auto processing = false;
        while (1) {
            std::unique_lock<std::mutex> lk(mtx);
            if (processing) { //処理中であれば、少し待って、様子を見に行く。
                cond.wait_for(lk, std::chrono::milliseconds(100));
            } else {
                cond.wait(lk);
            }

            if (stop_request) {
                break;
            }

            processing = func_buffer.callOnce(); //次のpublish,subscribe要求をロックしないように、すぐ抜ける。
        }
    }

private:

    std::thread th;
    std::mutex mtx;
    std::condition_variable cond;

    TopicFuncPairList func_buffer;

    bool stop_request = false;
};

#include "singleton.hpp"
class Broker{
public:
    static void run(){
        Singleton<BrokerCore>::getInstance().run();
    }

    static BrokerCore& getInstance(){
        return Singleton<BrokerCore>::getInstance();
    }

    static void stop(){
        Singleton<BrokerCore>::getInstance().stop();
        Singleton<BrokerCore>::destroy();
    }

private:
    Broker() = delete;
    ~Broker() = delete;
};
}
