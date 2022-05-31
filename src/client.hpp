#pragma once

#include "singleton.hpp"
#include "broker.hpp"

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

class PubSub {
public:
    PubSub() {

    }

    template<class ReturnType, class ClassType, class DataType>
    void subscribe(std::string topic, ReturnType (ClassType::*func_ptr)(DataType), ClassType *caller, size_t max_queue_size = 0) {
        Broker::getInstance().subscribe(topic, func_ptr, caller, max_queue_size);
    }

    template<class DataType>
    Publisher<DataType> getPublisher(std::string topic) {
        return Publisher<DataType>(topic);
    }

};

class PubSubAll {
public:
    PubSubAll() {

    }

    template<class ClassType>
    void subscribe(void (ClassType::*func_ptr)(std::string, std::string), ClassType *caller, size_t max_queue_size = 0, int sender_id = -1) {
        Broker::getInstance().addFunc(func_ptr, caller, max_queue_size, sender_id);
    }

    template<class DataType, class SerializerType>
    void setSerializer(std::string topic) {
        Broker::getInstance().setSerializer<DataType, SerializerType>(topic);
    }

    void publish(std::string topic, const std::string &msg, int sender_id) {
        Broker::getInstance().publish_msg(topic, msg, sender_id);
    }
};
