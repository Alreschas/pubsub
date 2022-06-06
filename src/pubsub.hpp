#pragma once

#include "singleton.hpp"
#include "broker.hpp"

namespace pubsub {

template<class DataType>
class Publisher {
public:
    Publisher(std::string topic) :
            topic(topic) {

    }

    void publish(const DataType &value) {
        Broker::getInstance().publish(topic, value);
    }

private:
    std::string topic;
};

class Subscriber{
public:
    Subscriber(Subscriber&& sub):topic(sub.topic),handler(sub.handler){
        sub.handler = 0;
    }

    Subscriber(){
    }

    ~Subscriber(){
        close();
    }

    void close(){
        if(handler == 0){
            return;
        }
        Broker::getInstance().close_subscribe(topic,handler);
    }

    void pause(){
        if(handler == 0){
            return;
        }
        Broker::getInstance().pause_subscribe(topic,handler);
    }

    void resume(){
        if(handler == 0){
            return;
        }
        Broker::getInstance().resume_subscribe(topic,handler);
    }

    pubsub::Subscriber& operator=(pubsub::Subscriber&&rhs){
        topic = rhs.topic;
        handler = rhs.handler;
        rhs.handler = 0;
        return *this;
    }
private:
    Subscriber(std::string topic,int handler):topic(topic),handler(handler){
    }

    Subscriber(const Subscriber& sub) = delete;
    Subscriber(Subscriber& sub) = delete;

private:
    std::string topic;
    unsigned int handler = 0;//!< 0は、無効値
    friend class api;
};

class api {
public:
    template<class ReturnType, class ClassType, class DataType>
    static Subscriber subscribe(const std::string &topic, ReturnType (ClassType::*func_ptr)(DataType), ClassType *caller, size_t max_queue_size = 0) {
        auto handler = Broker::getInstance().subscribe(topic, func_ptr, caller, max_queue_size);
        return Subscriber(topic, handler);
    }

    template<class DataType>
    static bool getLatestData(const std::string &topic, DataType &data) {
        return Broker::getInstance().getLatestData<DataType>(topic, data);
    }
private:
    api() = delete;
    ~api() = delete;
};

class Subscriber_serialized{
public:
    Subscriber_serialized(){

    }

    Subscriber_serialized(Subscriber_serialized&& sub):handler(sub.handler){
        sub.handler = 0;
    }

    pubsub::Subscriber_serialized& operator=(pubsub::Subscriber_serialized&&rhs){
        handler = rhs.handler;
        rhs.handler = 0;
        return *this;
    }

    ~Subscriber_serialized(){
        close();
    }

    void close(){
        if(handler == 0){
            return;
        }

        Broker::getInstance().close_subscribe_serialized(handler);
    }

private:
    Subscriber_serialized(int handler):handler(handler){
    }
private:
    unsigned int handler = 0;//!< 0は、無効値
    friend class extra_api;
};


class extra_api {
public:

    template<class ClassType>
    static Subscriber_serialized subscribe_serialized(void (ClassType::*func_ptr)(const std::string&, const std::string&), ClassType *caller, size_t max_queue_size = 0, int sender_id = -1) {
        int handler = Broker::getInstance().addFunc(func_ptr, caller, max_queue_size, sender_id);
        return Subscriber_serialized(handler);
    }

    template<class DataType, class SerializerType>
    static void setSerializer(std::string topic) {
        Broker::getInstance().setSerializer<DataType, SerializerType>(topic);
    }

    static void publish_serialized(std::string topic, const std::string &msg, int sender_id) {
        Broker::getInstance().publish_msg(topic, msg, sender_id);
    }
private:
    extra_api() = delete;
    ~extra_api() = delete;
};
}
