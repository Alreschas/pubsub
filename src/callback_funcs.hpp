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
#include <type_traits>

#include "serializer_holder.hpp"
#include "callback_funcs_base.hpp"

namespace pubsub {

template<class ReturnType, class DataType>
class CallbackFuncs: public CallbackFuncsBase {
    struct MsgType{
        DataType data;
        int sender_id;
    };
    struct FuncInfo {
        std::function<ReturnType(MsgType &msg)> func;  //!< コールバック関数
        QFuture<ReturnType> future; //!< コールバック実行結果取得
        unsigned long msg_idx = 0;  //!< 次に送信するメッセージのインデックス番号
        size_t max_sque_size = 0;   //!< コールバックメッセージキューの最大サイズ 0だと無限サイズ
        unsigned int handler = 0; //!< コールバック関数を特定するためのID
        bool active = true;
    };
    SerializerHolderBase<DataType> *serializer = nullptr;
    unsigned int cur_handler_id = 0;

public:
    CallbackFuncs(size_t max_que_size = 0) :
            max_rque_size(max_que_size) {
        auto functional = std::bind(&CallbackFuncs<ReturnType, DataType>::default_callback, this, std::placeholders::_1);
        add_func(functional);
    }

    ~CallbackFuncs(){

        for (auto &func : funcs) {
            func.future.waitForFinished();
        }

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
    template<class DataTypeWithRef>
    unsigned int add_func(const std::function<ReturnType(DataTypeWithRef)> &in_func, size_t max_que_size = 0) {
        std::lock_guard<std::mutex> lk(mtx);
        auto lambda = [=](MsgType &msg){in_func(msg.data);};
        FuncInfo info { lambda, QFuture<ReturnType>(), msg_que.size(), max_que_size, ++cur_handler_id  };
        funcs.push_back(info);

        return info.handler;
    }

    unsigned int add_func(const std::function<ReturnType(MsgType&)> &in_func, size_t max_que_size = 0) {
        std::lock_guard<std::mutex> lk(mtx);
        FuncInfo info { in_func, QFuture<ReturnType>(), msg_que.size(), max_que_size, ++cur_handler_id };
        funcs.push_back(info);

        return info.handler;
    }

    void remove_func(unsigned int handler) override{
        std::lock_guard<std::mutex> lk(mtx);
        auto itr = std::find_if(funcs.begin(),funcs.end(),[&](FuncInfo& info){return info.handler == handler;});
        if(itr == funcs.end()){
            return;
        }

        funcs.erase(itr);
    }


    /**
     * 指定された関数のコールバックを停止する
     */
    void pause_func(unsigned int  handler)override{
        std::lock_guard<std::mutex> lk(mtx);
        auto itr = std::find_if(funcs.begin(),funcs.end(),[&](FuncInfo& info){return info.handler == handler;});
        if(itr == funcs.end()){
            return;
        }

        itr->active = false;
    }

    /**
     * 指定された関数のコールバックを再開する
     */
    void resume_func(unsigned int  handler)override{
        std::lock_guard<std::mutex> lk(mtx);
        auto itr = std::find_if(funcs.begin(),funcs.end(),[&](FuncInfo& info){return info.handler == handler;});
        if(itr == funcs.end()){
            return;
        }

        itr->active = true;
    }

    bool getLatestData(DataType& data){
        std::lock_guard<std::mutex> lk(mtx);
        if (msg_que.size() != 0) {
            data = msg_que.back().data;
            return true;
        }
        return false;
    }
    /**
     * 受信メッセージキューが前に詰められた場合、送信メッセージの開始インデックス番号を調整する。
     */
    size_t get_new_sndmsg_idx(size_t prev_idx, size_t deleted_msg_num, size_t msg_que_size, size_t max_sque_size) {
        //とりあえずは、インデックスの取りうる最小値を入れておく。
        size_t new_idx = msg_que_size - std::min(max_sque_size, msg_que_size);
        if (prev_idx > deleted_msg_num + new_idx) {
            //キューの先頭を削除しても、インデックスの最小値を超えない場合は、そのままインデックスを引く。
            new_idx = prev_idx - deleted_msg_num;
        }
        return new_idx;
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

        for (size_t idx = 0; idx < oldest_idx_supposed_to_be_pub; ++idx) {
            msg_que.pop_front(); //不要になったメッセージを削除する。この前にメッセージを追加するので、最低一つはメッセージが残る
        }
        assert(msg_que.size() >= 1);

        for (auto &func : funcs) {
            size_t msg_size_max = (func.max_sque_size == 0 ? msg_que.size() : func.max_sque_size);
            func.msg_idx = get_new_sndmsg_idx(func.msg_idx,oldest_idx_supposed_to_be_pub,msg_que.size(),msg_size_max);
        }
        oldest_idx_supposed_to_be_pub = 0;

        if (max_rque_size > 0 && msg_que.size() > max_rque_size) {
            //受信キューのサイズが最大に達している場合

            msg_que.pop_front(); //古いものを一つ破棄する


            //バッファがずれたので、関数のメッセージ読み込み開始場所もずらしておく。
            for (auto &func : funcs) {
                if(!func.active){
                    func.msg_idx = msg_que.size();//不要だが、一応
                }else{
                    size_t msg_size_max = (func.max_sque_size == 0 ? msg_que.size() : func.max_sque_size);
                    func.msg_idx = get_new_sndmsg_idx(func.msg_idx,1,msg_que.size(),msg_size_max);
                }
            }
        } else {
            //受信キューのサイズが増えた場合

            for (auto &func : funcs) {
                if (!func.active) {
                    func.msg_idx = msg_que.size();
                } else {
                    //送信キューサイズよりも多くのデータが溜まった場合、最古のデータを破棄してインデックスを進める。
                    size_t msg_size_max = (func.max_sque_size == 0 ? msg_que.size() : func.max_sque_size);
                    if (func.msg_idx + msg_size_max < msg_que.size()) {
                        func.msg_idx = msg_que.size() - func.max_sque_size;
                    }
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

        //最古のメッセージを破棄可能かどうか。
        //一度のcallOnceで関数は最大一回呼ばれるので、最大一つのメッセージが破棄される可能性がある。
        bool oldest_msg_discardable = false;

        if (funcs.size() == 0) {
            oldest_idx_supposed_to_be_pub = msg_que.size();
        } else {
            for (auto &func : funcs) {
                if (!func.active) {
                    continue;
                }

                if (!func.future.isFinished()) {
                    processing = true;
                    continue;
                }

                if (func.msg_idx < msg_que.size()) {
                    func.future = QtConcurrent::run(QThreadPool::globalInstance(), func.func, msg_que[func.msg_idx]);        //msgは、この時点でコピーされる。
                    func.msg_idx++;

                    if (!oldest_msg_discardable && (func.msg_idx - 1) == oldest_idx_supposed_to_be_pub) {
                        //本関数が最古のメッセージを利用した場合、最古のメッセージを削除していいかを調べる
                        oldest_msg_discardable = true;
                        for (auto &func_other : funcs) {
                            oldest_msg_discardable = oldest_msg_discardable && (func_other.msg_idx != oldest_idx_supposed_to_be_pub);
                            if (!oldest_msg_discardable) {
                                break;
                            }
                        }

                        if (oldest_msg_discardable) {
                            oldest_idx_supposed_to_be_pub = func.msg_idx;
                        }
                    }
                    processing = true;
                }
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

    ReturnType default_callback(MsgType& msg) {
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
    size_t oldest_idx_supposed_to_be_pub = 0; //!< 送信予定の最古のメッセージ
};

}
