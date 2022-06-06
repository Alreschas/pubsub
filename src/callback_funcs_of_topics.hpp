#pragma once

#include <iostream>
#include <map>
#include <string>
#include <functional>
#include <type_traits>

#include "default_serializer.hpp"
#include "callback_funcs.hpp"

namespace pubsub {
/**
 * トピックとコールバック関数のリスト
 */
class TopicFuncPairList {
public:

    ~TopicFuncPairList() {
        for (auto &func : topic_funcs) {
            if (func.second) {
                delete func.second;
                func.second = nullptr;
            }
        }
    }

    template<class DataType>
    CallbackFuncs<void, DataType> * createOrGetFunc(std::string topic){
        CallbackFuncs<void, DataType> *func = nullptr;
        if (topic_funcs.count(topic) == 0) {
            func = new CallbackFuncs<void, DataType>();
            func->template setSerializer<defaultSerializer>();
            topic_funcs.emplace(topic, func);
            for (auto &gfunc : generalized_funcs) {
                auto functional = std::bind(gfunc.first, topic, std::placeholders::_1);
                func->addFunc(functional,gfunc.second);
            }
        } else {
            func = cast<void, DataType>(topic_funcs[topic]);
        }
        return func;
    }


    /**
     * コールバック関数を登録する
     */
    template<class DataTypeWithConstAndReference>
    unsigned short add(std::string topic, const std::function<void(DataTypeWithConstAndReference)> &in_func, size_t max_que_size = 0) {
        unsigned short ret = 0;
        using DataType = typename std::remove_const<typename std::remove_reference<DataTypeWithConstAndReference>::type>::type;
        auto *func = createOrGetFunc<DataType>(topic);
        if (func) {
            ret = func->add_func(in_func, max_que_size);
        }
        return ret;
    }

    void close_func(std::string topic, unsigned int handler){
        if (topic_funcs.count(topic) != 0) {
            topic_funcs[topic]->remove_func(handler);
        }
    }

    void pause_func(std::string topic, unsigned int handler){
        if (topic_funcs.count(topic) != 0) {
            topic_funcs[topic]->pause_func(handler);
        }
    }

    void resume_func(std::string topic, unsigned int handler){
        if (topic_funcs.count(topic) != 0) {
            topic_funcs[topic]->resume_func(handler);
        }
    }

    /**
     * シリアライザを登録する
     */
    template<class DataType, class SerializerType>
    void setSerializer(std::string topic) {
        CallbackFuncs<void, DataType> *func = createOrGetFunc<DataType>(topic);
        if (func) {
            func->template setSerializer<SerializerType>();
        }
    }

    /**
     * シリアライザ付きのコールバック関数を登録する
     */
    void addFunc(std::function<void(const std::string&, const std::string&)> func,int sender_id) {
        generalized_funcs.push_back(std::make_pair(func,sender_id));
        for (auto topic_func : topic_funcs) {
            auto functional = std::bind(func, topic_func.first, std::placeholders::_1);
            topic_func.second->addFunc(functional,sender_id);
        }
    }

    /**
     * データを更新する
     */
    template<class DataType>
    void add_data(std::string topic, const DataType &data) {
        CallbackFuncs<void, DataType> *func = createOrGetFunc<DataType>(topic);
        if (func) {
            func->add_data(data);
        }
    }

    void add_msg(std::string topic, const std::string &msg,int sender_id) {
        if (topic_funcs.count(topic) != 0) {
            topic_funcs[topic]->add_msg(msg,sender_id);
        }
    }


    /**
     * コールバック関数を、最大一回実行する。
     *
     */
    bool callOnce() {
        bool processing = false;
        for (auto &func : topic_funcs) {
            processing |= func.second->callOnce();
        }
        return processing;
    }

private:
    template<class ReturnType, class DataType>
    CallbackFuncs<ReturnType, DataType>* cast(CallbackFuncsBase *base) {
        return dynamic_cast<CallbackFuncs<ReturnType, DataType>*>(base);
    }


private:
    std::map<std::string, CallbackFuncsBase*> topic_funcs;

    std::vector<std::pair<std::function<void(std::string, std::string)>,int>> generalized_funcs;
};
}
