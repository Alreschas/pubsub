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

    virtual void close_subscribe(unsigned int handler) = 0;
    virtual void pause_subscribe(unsigned int handler) = 0;
    virtual void resume_subscribe(unsigned int handler) = 0;

    /**
     * シリアライザを利用する場合の、コールバック関数登録
     */
    virtual void subscribe_serialized(std::function<void(const std::string&)> func, int sender_id, unsigned int handler)=0;
    virtual void close_subscribe_serialized(unsigned int handler) = 0;
    virtual void publish_serialized(const std::string &msg,int sender_id) = 0;

};
}
