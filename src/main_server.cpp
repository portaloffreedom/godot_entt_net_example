//
// Created by matteo on 9/23/19.
//

#include "Server.h"
#include <iostream>

int main(int argc, char** argv)
{
    std::cout << "Server starting" << std::endl;
    Server server(1234);
    server.run();
    server.join();
    std::cout << "Server ended" << std::endl;
}
