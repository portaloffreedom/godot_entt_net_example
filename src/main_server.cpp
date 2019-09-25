//
// Created by matteo on 9/23/19.
//

#include "Server.h"
#include <iostream>
#include <google/protobuf/stubs/common.h>
#include "message.pb.h"

int main(int argc, char** argv)
{
    std::cout << "Server starting" << std::endl;
    Server server(1234);
    server.run();
    for (int i=0; i<100; i++)
    {
        Godot::GNSMessage message;
        message.set_type(Godot::MessageType::FRAME);
        Godot::Frame *frame = message.mutable_frame();
        frame->add_entities();
        Godot::Vector3 *pos = frame->mutable_entities(0)->mutable_position();
        Godot::Vector3 *vel = frame->mutable_entities(0)->mutable_velocity();
        std::string *name = frame->mutable_entities(0)->mutable_name();

        pos->set_x(0);
        pos->set_y(1);
        pos->set_z(2);

        vel->set_x(3);
        vel->set_y(4);
        vel->set_z(5);

        name->assign("diocane");

        server.send_message(message);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    server.join();
    std::cout << "Server ended" << std::endl;
    google::protobuf::ShutdownProtobufLibrary();
}
