//
// Created by matteo on 9/23/19.
//

#include "Client.h"
#include <iostream>

int main(int argc, char** argv)
{
    std::cout << "Client starting" << std::endl;
    Client client("127.0.0.1", 1234);
    client.run();
    client.join();
    std::cout << "Client ended" << std::endl;
}
