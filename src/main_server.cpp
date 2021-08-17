//
// Created by matteo on 9/23/19.
//

#include "Server.h"
#include <iostream>
#include "message_generated.h"

int main(int argc, char** argv)
{
    std::cout << "Server starting" << std::endl;
    std::shared_ptr<Network> network = std::make_shared<Network>();
    Server server(std::move(network), 1234);
    server.run();
    for (int i=0; i<100; i++)
    {
        auto builder = std::make_unique<flatbuffers::FlatBufferBuilder>();

        std::vector<flatbuffers::Offset<FlatGodot::Entity>> entities;
        FlatGodot::Vector3 pos(0,1,2);
        FlatGodot::Vector3 vel(3,4,5);

        entities.emplace_back(
                FlatGodot::CreateEntity(*builder, 42, &pos, &vel)
                );

        auto frame = FlatGodot::CreateFrame(*builder, builder->CreateVector(entities));
        auto message = FlatGodot::CreateGNSMessage(*builder, FlatGodot::MyMessage_Frame, frame.Union());
        builder->Finish(message);
        server.send_message(std::move(builder));
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    server.join();
    std::cout << "Server ended" << std::endl;
}
