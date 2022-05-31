#pragma once

#include <iostream>
#include <map>
#include <deque>
#include <string>
#include <mutex>
#include <functional>
#include <thread>
#include <unistd.h>

#include "callback_funcs.hpp"

class defaultSerializer {
public:
    template<class DataType>
    std::string serialize(DataType &data) {
        return data;
    }

    template<class DataType>
    DataType deserialize(const std::string &msg) {
        return DataType(msg);
    }
};

template<>
std::string defaultSerializer::serialize(int &data) {
    return std::to_string(data);
}

template<>
std::string defaultSerializer::serialize(double &data) {
    return std::to_string(data);
}

template<>
int defaultSerializer::deserialize<int>(const std::string &msg) {
    return std::stoi(msg);
}

template<>
double defaultSerializer::deserialize<double>(const std::string &msg) {
    return std::stod(msg);
}
