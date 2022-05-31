#pragma once

#include "singleton.hpp"
#include "broker.hpp"

class RealtimeDataClient {
public:
    RealtimeDataClient() {

    }

    template<class ReturnType, class ClassType, class DataType>
    void subscribe(std::string topic, ReturnType (ClassType::*func_ptr)(DataType), ClassType *caller, size_t max_queue_size = 0) {
        Broker::getInstance().subscribe(topic, func_ptr, caller, max_queue_size);
    }

    template<class DataType>
    void publish(std::string topic, const DataType &value) {
        Broker::getInstance().publish(topic, value);
    }
};

class RealtimeDataCommunicationClient{
public:
    RealtimeDataCommunicationClient(){

    }

    template<class ClassType>
    void subscribe(void(ClassType::*func_ptr)(std::string,std::string), ClassType *caller, size_t max_queue_size = 0) {
        Broker::getInstance().addFunc(func_ptr, caller, max_queue_size);
    }

    template<class DataType, class SerializerType>
    void setSerializer(std::string topic) {
        Broker::getInstance().setSerializer<DataType, SerializerType>(topic);
    }

    void publish(std::string topic, const std::string &msg) {
        Broker::getInstance().publish_msg(topic, msg);
    }
};
