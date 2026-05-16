#ifndef HELLOWORLDSTUBIMPL_HPP
#define HELLOWORLDSTUBIMPL_HPP

#include <iostream>
#include <CommonAPI/CommonAPI.hpp>
#include "v1/commonapi/HelloWorldStubDefault.hpp"

using namespace v1::commonapi;

class HelloWorldStubImpl : public HelloWorldStubDefault {
public:
    virtual void sayHello(const std::shared_ptr<CommonAPI::ClientId> _client,
                          std::string _name,
                          sayHelloReply_t _reply) override {
        std::stringstream messageStream;
        messageStream << "Hello " << _name << " from QNX Server!";
        std::cout << "sayHello('" << _name << "'): " << messageStream.str() << std::endl;
        _reply(messageStream.str());
    }
};

#endif