#pragma once

#include <iostream>
#include <map>
#include <deque>
#include <string>
#include <mutex>
#include <functional>
#include <thread>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>
#include <unistd.h>

#include "serializer_holder.hpp"
#include "default_serializer.hpp"

class CallbackFuncsBase {
public:
    virtual ~CallbackFuncsBase() {
    }
    virtual bool callOnce() = 0;

    /**
     * シリアライザを利用する場合の、コールバック関数登録
     */
    void addFunc(std::function<void(std::string)> func,int sender_id) {
        FuncInfo_generalized info;
        info.func = func;
        info.sender_id = sender_id;
        funcs_generalized.push_back(info);
    }

    virtual void add_msg(const std::string &msg,int sender_id) = 0;

protected:

    /** データ形式によらないコールバック関数型
     *
     */
    struct FuncInfo_generalized {
        int sender_id = -1;
        std::function<void(std::string)> func;  //!< コールバック関数
    };
    std::vector<FuncInfo_generalized> funcs_generalized;
};

template<class ReturnType, class DataType>
class CallbackFuncs: public CallbackFuncsBase {
    struct MsgType{
        DataType data;
        int sender_id;
    };
    struct FuncInfo {
        std::function<ReturnType(MsgType msg)> func;  //!< コールバック関数
        QFuture<ReturnType> future;         //!< コールバック実行結果取得
        unsigned long msg_idx = 0; //!< 次に送信するメッセージのインデックス番号
        size_t max_sque_size = 0;   //!< コールバックメッセージキューの最大サイズ 0だと無限サイズ
    };
    SerializerHolderBase<DataType> *serializer = nullptr;

public:
    CallbackFuncs(size_t max_que_size = 0) :
            max_rque_size(max_que_size) {
        auto functional = std::bind(&CallbackFuncs<ReturnType, DataType>::default_callback, this, std::placeholders::_1);
        add_func(functional);
    }

    ~CallbackFuncs(){
        if(serializer){
            delete serializer;
            serializer = nullptr;
        }
    }

    template<class SerializerType>
    void setSerializer() {
        if (this->serializer) {
            delete this->serializer;
            this->serializer = nullptr;
        }
        this->serializer = new SerializerHolder<SerializerType, DataType>();
    }

    /**
     * コールバック関数を登録する
     */
    void add_func(const std::function<ReturnType(DataType)> &in_func, size_t max_que_size = 0) {
        std::lock_guard<std::mutex> lk(mtx);
        auto lambda = [=](MsgType msg){in_func(msg.data);};
        FuncInfo info { lambda, QFuture<ReturnType>(), msg_que.size(), max_que_size };
        funcs.push_back(info);
    }

    void add_func(const std::function<ReturnType(MsgType)> &in_func, size_t max_que_size = 0) {
        std::lock_guard<std::mutex> lk(mtx);
        FuncInfo info { in_func, QFuture<ReturnType>(), msg_que.size(), max_que_size };
        funcs.push_back(info);
    }

    /**
     * コールバックメッセージを保存する
     */
    void add_data(const DataType &data,int sender_id = -1) {
        std::lock_guard<std::mutex> lk(mtx);

        MsgType msg;
        msg.data = data;
        msg.sender_id = sender_id;
        msg_que.push_back(msg);
        if (max_rque_size > 0 && msg_que.size() > max_rque_size) {
            msg_que.pop_front(); //受信キューのサイズを超えてしまった場合は、古いものを一つ破棄する

            //バッファがずれたので、関数のメッセージ読み込み開始場所もずらしておく。
            for (auto &func : funcs) {
                size_t msg_size_max = (func.max_sque_size == 0 ? msg_que.size() : func.max_sque_size);
                if (func.msg_idx > 0 && func.msg_idx > msg_que.size() - msg_size_max) {
                    func.msg_idx--;
                }
            }
        } else {
            //バッファが増えたことによって受信キューサイズよりもインデックスが小さくなった場合は、大きくしておく
            for (auto &func : funcs) {
                size_t msg_size_max = (func.max_sque_size == 0 ? msg_que.size() : func.max_sque_size);
                if (static_cast<int>(func.msg_idx) < static_cast<long>(msg_que.size() - msg_size_max)) {
                    func.msg_idx = msg_que.size() - func.max_sque_size;
                }
            }
        }
    }



    /**
     * 各関数に対して、コールバックメッセージがある場合は、一度だけコールバック関数を呼び出す。
     *
     * \detail コールバック関数は、Qtのスレッドプールで実行する。
     *
     * \return コールバック関数実行中かどうか
     */
    bool callOnce() {
        std::lock_guard<std::mutex> lk(mtx);
        bool processing = false;
        for (auto &func : funcs) {
            if (!func.future.isFinished()) {
                processing = true;
                continue;
            }

            if (func.msg_idx < msg_que.size()) {
                auto msg = msg_que[func.msg_idx];
                func.future = QtConcurrent::run(QThreadPool::globalInstance(), func.func, msg);
                func.msg_idx++;
                processing = true;
            }
        }
        return processing;
    }

    void add_msg(const std::string &msg,int sender_id){
        if (serializer) {
            auto data = serializer->deserialize(msg);
            add_data(data,sender_id);
        }
    }

    ReturnType default_callback(MsgType msg) {
        if(!serializer){
            return ReturnType();
        }
        std::string data = serializer->serialize(msg.data);

        for (auto &func : funcs_generalized) {
            if (func.func&&func.sender_id != msg.sender_id) {
                func.func(data);
            }
        }
        return ReturnType();
    }

private:
    std::mutex mtx;
    std::vector<FuncInfo> funcs;
    std::deque<MsgType> msg_que; //!< メッセージ受信キュー
    size_t max_rque_size = 0; //!< メッセージ受信キューの最大サイズ 0だと、無限サイズ
};

/**
 * トピックとコールバック関数のリスト
 */
class CallbackFuncsOfTopics {
public:

    ~CallbackFuncsOfTopics() {
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
    template<class DataType>
    void add(std::string topic, const std::function<void(DataType)> &in_func, size_t max_que_size = 0) {
        CallbackFuncs<void, DataType> *func = createOrGetFunc<DataType>(topic);
        if (func) {
            func->add_func(in_func, max_que_size);
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
    void addFunc(std::function<void(std::string, std::string)> func,int sender_id) {
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
