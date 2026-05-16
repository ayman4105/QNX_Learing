#include <iostream>
#include <thread>
#include <chrono>
#include <CommonAPI/CommonAPI.hpp>
#include "v1/commonapi/HelloWorldProxy.hpp"

using namespace v1::commonapi;

int main() {
    // "local" replaced with "commonapi.HelloWorld" domain for network SOME/IP
    std::shared_ptr<CommonAPI::Runtime> runtime = CommonAPI::Runtime::get();
    std::shared_ptr<HelloWorldProxy<>> myProxy =
        runtime->buildProxy<HelloWorldProxy>("local", "commonapi.HelloWorld", "HelloWorldClient");

    if (!myProxy) {
        std::cerr << "Failed to create proxy!" << std::endl;
        return 1;
    }

    std::cout << "QNX Client: Waiting for service..." << std::endl;

    int timeout = 100;
    while (!myProxy->isAvailable() && timeout > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        timeout--;
    }

    if (!myProxy->isAvailable()) {
        std::cerr << "Service not available after timeout!" << std::endl;
        return 1;
    }

    std::cout << "Service available! Starting periodic calls..." << std::endl;

    int counter = 0;
    while (true) {
        CommonAPI::CallStatus callStatus;
        std::string returnMessage;
        std::string name = "QNX_RPi5_Ayman!!" + std::to_string(counter);

        myProxy->sayHello(name, callStatus, returnMessage);

        if (callStatus == CommonAPI::CallStatus::SUCCESS) {
            std::cout << "[" << counter << "] Got message: '"
                      << returnMessage << "'" << std::endl;
        } else {
            std::cout << "[" << counter << "] Call failed: "
                      << (int)callStatus << std::endl;
        }

        counter++;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
