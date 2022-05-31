#include <iostream>

#include "client.hpp"
#include "broker.hpp"

std::mutex mtx;

class alldataSubscriber {
public:
    alldataSubscriber() {
        PubSubAll client;
        client.subscribe(&alldataSubscriber::cbFunc1, this, 10, 1);
        client.subscribe(&alldataSubscriber::cbFunc2, this, 10, 2);
    }

    void cbFunc1(std::string topic, std::string msg) {
        std::lock_guard<std::mutex> lk(mtx);
        std::cout << "\033[31m" << "all sub1 : " << topic << " " << msg << "\033[m" << std::endl;
    }
    void cbFunc2(std::string topic, std::string msg) {
        std::lock_guard<std::mutex> lk(mtx);
        std::cout << "\033[32m" << "all sub2 : " << topic << " " << msg << "\033[m" << std::endl;
    }

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
        PubSubAll client;
        for (int idx = 0; idx < 1; ++idx) {
            client.publish("/str", "published from alldata publisher", 1);
            client.publish("/int", "9999", 1);
            client.publish("/dbl", "9999.9999", 1);
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
        PubSub client;
        auto str_publisher = client.getPublisher<std::string>("/str");
        auto dbl_publisher = client.getPublisher<double>("/dbl");
        auto int_publisher = client.getPublisher<int>("/int");

        for (int idx = 0; idx < 1; ++idx) {
            str_publisher.publish("published from test sender.");
            int_publisher.publish(12345);
            dbl_publisher.publish(1.2345);

            usleep(100000);
        }
    }

    std::thread th;
};

class TestReceiver {
public:
    TestReceiver() {
        PubSub client;
        client.subscribe("/int", &TestReceiver::intSubscriber, this, 4);
        client.subscribe("/str", &TestReceiver::stringSubscriber, this, 4);
        client.subscribe("/dbl", &TestReceiver::doubleSubscriber, this, 4);
    }

    void intSubscriber(int a) {
        mtx.lock();
        std::cout << "\033[33m" << "int sub  : " << a << "\033[m" << std::endl;
        mtx.unlock();
    }

    void stringSubscriber(std::string a) {
        mtx.lock();
        std::cout << "\033[33m" << "str sub  : " << a << "\033[m" << std::endl;
        mtx.unlock();
    }

    void doubleSubscriber(double a) {
        mtx.lock();
        std::cout << "\033[33m" << "dbl sub  : " << a << "\033[m" << std::endl;
        mtx.unlock();
    }
};

int main() {
    Broker::run();

    alldataSubscriber ws_sender;
    TestReceiver receiver;

    std::cout << "===== network receive demo===== " << std::endl;
    alldataPublisher ws_receiver;
    ws_receiver.wait();

    std::cout << "=====  topic publishing demo ===== " << std::endl;
    TestSender sender;
    sender.wait();

    usleep(1000000);
    Broker::stop();

    return 0;
}
