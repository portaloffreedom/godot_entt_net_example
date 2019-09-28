//
// Created by matteo on 9/23/19.
//

#include "Client.h"
#include <iostream>
#include <google/protobuf/stubs/common.h>

int main(int argc, char** argv)
{
    std::cout << "Client starting" << std::endl;
    std::shared_ptr<Network> network = std::make_shared<Network>();
    Client client(std::move(network), "127.0.0.1", 1234);
    client.run();
    client.join();
    std::cout << "Client ended" << std::endl;

    google::protobuf::ShutdownProtobufLibrary();
}
