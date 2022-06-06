#include <iostream>

#include "pubsub.hpp"
#include "broker.hpp"

std::mutex mtx;

class alldataSubscriber {
public:
    alldataSubscriber() {
        sub1 = pubsub::extra_api::subscribe_serialized(&alldataSubscriber::cbFunc1, this, 10, 1);
        sub2 = pubsub::extra_api::subscribe_serialized(&alldataSubscriber::cbFunc2, this, 10, 2);
    }

    void cbFunc1(const std::string &topic, const std::string &msg) {
        std::lock_guard<std::mutex> lk(mtx);
        std::cout << "\033[31m" << "all sub1 : " << topic << " " << msg << "\033[m" << std::endl;
    }
    void cbFunc2(const std::string &topic, const std::string &msg) {
        std::lock_guard<std::mutex> lk(mtx);
        std::cout << "\033[32m" << "all sub2 : " << topic << " " << msg << "\033[m" << std::endl;
    }

private:
    pubsub::Subscriber_serialized sub1;
    pubsub::Subscriber_serialized sub2;
};

class alldataPublisher {
public:
    alldataPublisher() {
        th = std::thread(&alldataPublisher::receiveThread, this);
    }

    ~alldataPublisher() {
        if (th.joinable()) {
            th.join();
        }
    }

    void wait() {
        if (th.joinable()) {
            th.join();
        }
    }

    void receiveThread() {
        pubsub::Publisher<std::string> str_pub("/str");
        pubsub::Publisher<int>         int_pub("/int");
        pubsub::Publisher<double>      dbl_pub("/dbl");

        for (int idx = 0; idx < 1; ++idx) {
            pubsub::extra_api::publish_serialized("/str", "published from alldata publisher", 1);
            pubsub::extra_api::publish_serialized("/int", "9999", 1);
            pubsub::extra_api::publish_serialized("/dbl", "9999.9999", 1);
            usleep(100000);
        }
    }

    std::thread th;
};

class TestSender {
public:
    TestSender() {
        th = std::thread(&TestSender::sendThread, this);
    }

    ~TestSender() {
        if (th.joinable()) {
            th.join();
        }
    }

    void wait() {
        if (th.joinable()) {
            th.join();
        }
    }

    void sendThread() {
        pubsub::Publisher<std::string> str_pub("/str");
        pubsub::Publisher<int>         int_pub("/int");
        pubsub::Publisher<double>      dbl_pub("/dbl");

        for (int idx = 0; idx < 10; ++idx) {
            str_pub.publish("published from test sender."+std::to_string(idx));
            int_pub.publish(10+idx);
            dbl_pub.publish(10.1+idx);

            usleep(100000);
        }
    }

    std::thread th;
};

class TestReceiver {
public:
    TestReceiver() {
        intsub = pubsub::api::subscribe("/int", &TestReceiver::intSubscriber, this, 4);
        strsub = pubsub::api::subscribe("/str", &TestReceiver::stringSubscriber, this, 4);
        dblsub = pubsub::api::subscribe("/dbl", &TestReceiver::doubleSubscriber, this, 4);
    }

    void intSubscriber(const int &a) {
        mtx.lock();
        std::cout << "\033[33m" << "int sub  : " << a << "\033[m" << std::endl;
        mtx.unlock();
        intsub.pause();
    }

    void stringSubscriber(std::string &a) {
        mtx.lock();
        std::cout << "\033[33m" << "str sub  : " << a << "\033[m" << std::endl;
        mtx.unlock();
        strsub.pause();
    }

    void doubleSubscriber(double a) {
        mtx.lock();
        std::cout << "\033[33m" << "dbl sub  : " << a << "\033[m" << std::endl;
        mtx.unlock();
        dblsub.pause();
    }

private:
    pubsub::Subscriber intsub;
    pubsub::Subscriber dblsub;
    pubsub::Subscriber strsub;
};

int main() {
    pubsub::Broker::run();

    alldataSubscriber ws_sender;
    TestReceiver receiver;

    std::cout << "===== network receive demo===== " << std::endl;
    alldataPublisher ws_receiver;
    ws_receiver.wait();

    std::cout << "=====  topic publishing demo ===== " << std::endl;
    TestSender sender;
    sender.wait();

    std::cout << "=====  get latestdata demo ===== " << std::endl;
    int data = 0;
    pubsub::api::getLatestData<int>("/int", data);
    std::cout<<"latest int data: "<<data<<std::endl;

    pubsub::Broker::stop();

    return 0;
}
