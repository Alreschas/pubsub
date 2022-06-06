#pragma once

#include <iostream>

namespace pubsub {
template<class DataType>
class SerializerHolderBase {
public:
    SerializerHolderBase() {
    }
    virtual ~SerializerHolderBase() {
    }
    virtual std::string serialize(DataType &data) = 0;
    virtual DataType deserialize(const std::string &msg) = 0;
};

template<class SerializerType, class DataType>
class SerializerHolder: public SerializerHolderBase<DataType> {
public:
    SerializerHolder() {
    }

    std::string serialize(DataType &data) override {
        return serializer.serialize(data);
    }

    DataType deserialize(const std::string &msg) override {
        return serializer.template deserialize<DataType>(msg);
    }
private:
    SerializerType serializer;
};
}
