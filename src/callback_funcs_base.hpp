#pragma once

#include <iostream>
#include <vector>
#include <functional>

namespace pubsub {
class CallbackFuncsBase {
public:
    virtual ~CallbackFuncsBase() {
    }
    virtual bool callOnce() = 0;

    /**
     * シリアライザを利用する場合の、コールバック関数登録
     */
    void addFunc(std::function<void(const std::string&)> func,int sender_id,unsigned int handler) {
        FuncInfo_generalized info { func, sender_id, handler };
        funcs_generalized.push_back(info);
    }

    void removeFunc(unsigned int handler){
        auto itr = std::find_if(funcs_generalized.begin(),funcs_generalized.end(),[&](const FuncInfo_generalized& info){return info.handler == handler;});
        if(itr != funcs_generalized.end()){
            funcs_generalized.erase(itr);
        }
    }

    virtual void add_msg(const std::string &msg,int sender_id) = 0;

    virtual void remove_func(unsigned int handler) = 0;
    virtual void pause_func(unsigned int handler) = 0;
    virtual void resume_func(unsigned int handler) = 0;

protected:

    /** データ形式によらないコールバック関数型
     *
     */
    struct FuncInfo_generalized {
        std::function<void(const std::string&)> func;  //!< コールバック関数
        int sender_id = -1;
        unsigned int handler;
    };
    std::vector<FuncInfo_generalized> funcs_generalized;
};
}
