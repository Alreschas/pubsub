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
    /**
     * シリアライズされたデータを扱う関数を格納する
     */
    struct FuncSerializedData{
        std::function<void(std::string, std::string)> func;
        int except_sender = 0; //!< 送信してきた相手に、再度送信するのを防ぐためのID
        unsigned int handler = 0; //!< 関数を停止したりするためのハンドラ
        size_t max_queue_size = 0;
    };

public:

    ~TopicFuncPairList() {
        for (auto &func : topic_funcs) {
            if (func.second) {
                delete func.second;
                func.second = nullptr;
            }
        }
    }

    /**
     * コールバック関数を登録する
     */
    template<class DataTypeWithConstAndReference>
    unsigned short subscribe(const std::string &topic, const std::function<void(DataTypeWithConstAndReference)> &in_func, size_t max_que_size = 0) {
        unsigned short ret = 0;
        using DataType = typename std::remove_const<typename std::remove_reference<DataTypeWithConstAndReference>::type>::type;
        auto *func = createOrGetFunc<DataType>(topic);
        if (func) {
            ret = func->subscribe(in_func, max_que_size);
        }
        return ret;
    }

    void close_subscribe(const std::string &topic, unsigned int handler){
        if (topic_funcs.count(topic) != 0) {
            topic_funcs[topic]->close_subscribe(handler);
        }
    }

    void pause_subscribe(const std::string &topic, unsigned int handler){
        if (topic_funcs.count(topic) != 0) {
            topic_funcs[topic]->pause_subscribe(handler);
        }
    }

    void resume_subscribe(const std::string &topic, unsigned int handler){
        if (topic_funcs.count(topic) != 0) {
            topic_funcs[topic]->resume_subscribe(handler);
        }
    }

    template<class DataType>
    bool getLatestData(const std::string &topic, DataType& data){
        auto func = getFunc<DataType>(topic);
        if(func){
            return func->getLatestData(data);
        }
        return false;
    }

    /**
     * シリアライザ付きのコールバック関数を登録する
     */
    int subscribe_serialized(std::function<void(const std::string&, const std::string&)> func, size_t max_queue_size, int except_sender) {
        FuncSerializedData func_info { func, except_sender, ++cur_handler, max_queue_size };
        generalized_funcs.push_back(func_info);
        for (auto topic_func : topic_funcs) {
            auto functional = std::bind(func_info.func, topic_func.first, std::placeholders::_1);
            topic_func.second->subscribe_serialized(functional, func_info.except_sender, func_info.handler ,max_queue_size);
        }
        return func_info.handler;
    }

    void close_subscribe_serialized(unsigned int handler) {
        for (auto topic_func : topic_funcs) {
            topic_func.second->close_subscribe_serialized(handler);
        }

        auto itr = std::find_if(generalized_funcs.begin(), generalized_funcs.end(), [&](const FuncSerializedData &info) {return info.handler == handler;});
        if (itr != generalized_funcs.end()) {
            generalized_funcs.erase(itr);
        }
    }


    /**
     * シリアライザを登録する
     */
    template<class DataType, class SerializerType>
    void setSerializer(const std::string &topic) {
        CallbackFuncs<void, DataType> *func = createOrGetFunc<DataType>(topic);
        if (func) {
            func->template setSerializer<SerializerType>();
        }
    }


    /**
     * データを更新する
     */
    template<class DataType>
    void publish(const std::string &topic, const DataType &data, SendType type) {
        CallbackFuncs<void, DataType> *func = createOrGetFunc<DataType>(topic);
        if (func) {
            func->publish(data, type, NO_EXCEPT);
        }
    }

    void publish_serialized(const std::string &topic, const std::string &msg, SendType type, int sender_id) {
        if (topic_funcs.count(topic) != 0) {
            topic_funcs[topic]->publish_serialized(msg, type, sender_id);
        }
    }

    /**
     * 各トピックのコールバック関数を、最大一回実行する。
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
    template<class DataType>
    CallbackFuncs<void, DataType> * createOrGetFunc(const std::string &topic){
        CallbackFuncs<void, DataType> *func = nullptr;
        if (topic_funcs.count(topic) == 0) {
            func = new CallbackFuncs<void, DataType>();
            func->template setSerializer<defaultSerializer>();
            topic_funcs.emplace(topic, func);
            for (auto &gfunc : generalized_funcs) {
                auto functional = std::bind(gfunc.func, topic, std::placeholders::_1);
                func->subscribe_serialized(functional, gfunc.except_sender, gfunc.handler, gfunc.max_queue_size);
            }
        } else {
            func = cast<void, DataType>(topic_funcs[topic]);
        }
        return func;
    }


    template<class DataType>
    CallbackFuncs<void, DataType> * getFunc(const std::string &topic){
        CallbackFuncs<void, DataType> *func = nullptr;
        if (topic_funcs.count(topic) != 0) {
            func = cast<void, DataType>(topic_funcs[topic]);
        }
        return func;
    }

    template<class ReturnType, class DataType>
    CallbackFuncs<ReturnType, DataType>* cast(CallbackFuncsBase *base) {
        return dynamic_cast<CallbackFuncs<ReturnType, DataType>*>(base);
    }


private:
    std::map<std::string, CallbackFuncsBase*> topic_funcs;

    unsigned int cur_handler = 0; //!< シリアライズ付きのデータサブスクライブ関数を特定するハンドラ
    std::vector<FuncSerializedData> generalized_funcs;
};
}
