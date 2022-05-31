#pragma once

template<class T>
class Singleton {
public:
    static T& getInstance() {
        if (!instance) {
            create();
        }
        return *instance;
    }

    static void destroy() {
        delete instance;
        instance = nullptr;
    }

private:
    static void create() {
        instance = new T();
    }

    Singleton() = delete;
    ~Singleton() = delete;
private:
    static T *instance;
};

template<class T> T* Singleton<T>::instance = nullptr;
