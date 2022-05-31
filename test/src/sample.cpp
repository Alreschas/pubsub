#include <iostream>

#include "client.hpp"
#include "broker.hpp"

std::mutex mtx;

class dummyNetworkSender{
public:
    dummyNetworkSender() {
        RealtimeDataCommunicationClient client;
        client.subscribe(&dummyNetworkSender::cbFunc, this);
    }


    void cbFunc(std::string topic,std::string msg){
        std::lock_guard<std::mutex> lk(mtx);
        std::cout<<"dummyNetworkSender : "<<topic<<" "<<msg<<std::endl;
        //ここで、サーバに送信
    }

};

class dummyNetworkReceiver{
public:
    dummyNetworkReceiver() {
        th = std::thread(&dummyNetworkReceiver::receiveThread,this);
    }

    ~dummyNetworkReceiver(){
        if(th.joinable()){
            th.join();
        }
    }


    void receiveThread(){
        RealtimeDataCommunicationClient client;
        for (int idx = 0; idx < 10; ++idx) {
            client.publish("/string", "received");
            client.publish("/int", "10");
            client.publish("/double", "12.0");
        }
    }


    std::thread th;
};

class TestSender{
public:
    TestSender(){
        th = std::thread(&TestSender::sendThread,this);
    }

    ~TestSender(){
        if(th.joinable()){
            th.join();
        }
    }

    void sendThread(){
        for (int idx = 0; idx < 10; ++idx) {
            RealtimeDataClient br;
            br.publish<std::string>("/string", "this is a pen.");
            br.publish<int>("/int", 1);
            br.publish<double>("/double", 1.2);

            usleep(1000);
        }
    }

    std::thread th;
};

class TestReceiver{
public:
    TestReceiver(){
        RealtimeDataClient br;
        br.subscribe("/int", &TestReceiver::intReceiver, this, 4);
        br.subscribe("/string", &TestReceiver::stringReceiver, this, 4);
        br.subscribe("/double", &TestReceiver::doubleReceiver, this, 4);
    }

    void exec(){

        //サーバが受信
        RealtimeDataCommunicationClient client;
        client.publish("/task", "this is a ship.");
        client.publish("/task", "these are two ships.");
        client.publish("/task", "these are three ships.");
        client.publish("/task", "these are four ships.");

    }

    void intReceiver(int a){
        mtx.lock();
        std::cout<<"intReceiver "<<a<<std::endl;
        mtx.unlock();
        usleep(100000);
    }

    void stringReceiver(std::string a){
        mtx.lock();
        std::cout<<"stringReceiver "<<a<<std::endl;
        mtx.unlock();
        usleep(200000);
    }

    void doubleReceiver(double a){
        mtx.lock();
        std::cout<<"doubleReceiver "<<a<<std::endl;
        mtx.unlock();
        usleep(300000);
    }
};


int main() {
    Broker::run();

    dummyNetworkSender ws_sender;
    TestReceiver receiver;

    dummyNetworkReceiver ws_receiver;
    TestSender sender;


    usleep(1000000);
    Broker::stop();

    return 0;
}
