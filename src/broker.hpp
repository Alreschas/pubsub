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


/**
 * メッセージの出版・購読処理を実行する。
 *
 */
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

    /**
     * メッセージの購読を開始する
     *
     * ここで開始した以降に出版されたメッセージから、購読が開始される。
     *
     */
    template<class ClassType, class DataTypeWithConstAndReference>
    unsigned int subscribe(const std::string &topic, void (ClassType::*func_ptr)(DataTypeWithConstAndReference), ClassType *caller, size_t max_que_size = 0) {
        std::lock_guard<std::mutex> lk(mtx);
        std::function<void(DataTypeWithConstAndReference)> functional = std::bind(func_ptr, caller, std::placeholders::_1);

        return func_buffer.subscribe(topic, functional, max_que_size);
    }

    /**
     * 最新のメッセージを取得する
     */
    template<class DataType>
    bool getLatestData(const std::string &topic, DataType &data) {
        std::lock_guard<std::mutex> lk(mtx);
        return func_buffer.getLatestData<DataType>(topic, data);
    }


    /**
     * メッセージの購読を閉じる
     */
    void close_subscribe(const std::string &topic, unsigned int handler) {
        std::lock_guard<std::mutex> lk(mtx);
        func_buffer.close_subscribe(topic, handler);
    }


    /**
     * メッセージの購読を一時停止する
     */
    void pause_subscribe(const std::string &topic, unsigned int handler) {
        std::lock_guard<std::mutex> lk(mtx);
        func_buffer.pause_subscribe(topic, handler);
    }

    /**
     * メッセージの購読を再開する
     */
    void resume_subscribe(const std::string &topic, unsigned int handler) {
        std::lock_guard<std::mutex> lk(mtx);
        func_buffer.resume_subscribe(topic, handler);
    }


    /**
     * メッセージを出版する
     */
    template<class DataType>
    void publish(const std::string &topic, const DataType &value) {
        std::lock_guard<std::mutex> lk(mtx);

        func_buffer.publish(topic, value);

        cond.notify_one();
    }

    /**
     * シリアライズされたメッセージを出版する
     */
    void publish_serialized(const std::string &topic, const std::string &msg,int sender_id) {
        std::lock_guard<std::mutex> lk(mtx);

        func_buffer.publish_serialized(topic, msg,sender_id);

        cond.notify_one();
    }

    /**
     * シリアライズされた全トピックのメッセージを購読する
     *
     * 購読を開始した時点で、最新のメッセージが一つ受信される。
     */
    template<class ClassType>
    int subscribe_serialized(void(ClassType::*func_ptr)(const std::string&,const std::string&), ClassType *caller, size_t max_queue_size = 0,int sender_id = -1) {
        std::lock_guard<std::mutex> lk(mtx);
        auto functional = std::bind(func_ptr, caller, std::placeholders::_1, std::placeholders::_2);
        cond.notify_one();
        return func_buffer.subscribe_serialized(functional,sender_id);
    }


    /**
     * subscribe_serializedで登録した購読を破棄する
     */
    void close_subscribe_serialized(unsigned int handler) {
        std::lock_guard<std::mutex> lk(mtx);
        func_buffer.close_subscribe_serialized(handler);
    }

    /**
     * トピックごとの、シリアライザを設定する
     */
    template<class DataType, class SerializerType>
    void setSerializer(const std::string &topic) {
        std::lock_guard<std::mutex> lk(mtx);
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

    TopicFuncPairList func_buffer; //!< 各トピックと、関数のリスト

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
