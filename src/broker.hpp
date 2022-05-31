#pragma once

#include <iostream>
#include <map>
#include <deque>
#include <string>
#include <mutex>
#include <functional>
#include <thread>
#include <unistd.h>

#include "callback_funcs.hpp"

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

    template<class ClassType, class DataType>
    void subscribe(std::string topic, void (ClassType::*func_ptr)(DataType), ClassType *caller, size_t max_que_size = 0) {
        std::lock_guard<std::mutex> lk(mtx);
        std::function<void(DataType)> functional = std::bind(func_ptr, caller, std::placeholders::_1);

        func_buffer.add(topic, functional, max_que_size);
    }

    template<class DataType>
    void publish(std::string topic, const DataType &value) {
        std::lock_guard<std::mutex> lk(mtx);

        func_buffer.add_data(topic, value);

        cond.notify_one();
    }

    void publish_msg(std::string topic, const std::string &msg) {
        std::lock_guard<std::mutex> lk(mtx);

        func_buffer.add_msg(topic, msg);

        cond.notify_one();
    }

    template<class ClassType>
    void addFunc(void(ClassType::*func_ptr)(std::string,std::string), ClassType *caller, size_t max_queue_size = 0) {
        auto functional = std::bind(func_ptr, caller, std::placeholders::_1, std::placeholders::_2);
        func_buffer.addFunc(functional);
    }

    template<class DataType, class SerializerType>
    void setSerializer(std::string topic) {
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

    CallbackFuncsOfTopics func_buffer;

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
