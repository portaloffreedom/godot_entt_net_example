#include <sstream>
#include <PackedScene.hpp>
#include <ResourceLoader.hpp>
#include <Input.hpp>
#include "EntityManager.h"
#include "message.pb.h"

using namespace godot;

typedef Vector3 position;

struct velocity
{
    float dx;
    float dy;
    float dz;
};

void EntityManager::_register_methods()
{
    register_method("_process", &EntityManager::_process);
}

EntityManager::EntityManager()
    : rd()
    , gen(rd())
    , dis(-5.0, 5.0)
    , server(1234)
{}

EntityManager::~EntityManager()
{
    server.close();
    google::protobuf::ShutdownProtobufLibrary();
}

void EntityManager::_init()
{
    this->entity_scene = ResourceLoader::get_singleton()->load("res://models/Entity.tscn");
    this->entity_script = ResourceLoader::get_singleton()->load("res://src/Entity.gdns");
    this->create_entity();
    this->server.run();
}

void EntityManager::_process(float delta)
{
    registry.view<position, velocity, Spatial*>().each([delta](position &pos, velocity &vel, Spatial* s_entity) {
        pos.x += vel.dx * delta;
        pos.y += vel.dy * delta;
        pos.z += vel.dz * delta;
        s_entity->set_translation(Vector3(pos.x, pos.y, pos.z));
    });

    for(auto &entity: registry.view<position, Spatial*>())
    {
        const float arena_size = 36.0f;
        auto &pos = registry.get<position>(entity);
        if (fabs(pos.x) > arena_size or fabs(pos.y) > arena_size)
        {
            registry.get<Spatial*>(entity)->queue_free();
            registry.destroy(entity);
        }
    }

    // network
    ::Godot::GNSMessage message;
    message.set_type(::Godot::MessageType::FRAME);
    ::Godot::Frame *frame = message.mutable_frame();
    int index = 0;
    registry.view<position, velocity, Spatial*>().each([frame, &index](position &pos, velocity &vel, Spatial* s_entity) {
        frame->add_entities();
        ::Godot::Entity *entity = frame->mutable_entities(index++);

        entity->mutable_position()->set_x(pos.x);
        entity->mutable_position()->set_y(pos.y);
        entity->mutable_position()->set_z(pos.z);
        entity->mutable_velocity()->set_x(vel.dx);
        entity->mutable_velocity()->set_y(vel.dy);
        entity->mutable_velocity()->set_z(vel.dz);
        std::string *name = entity->mutable_name();
        auto gs = s_entity->get_name().utf8();
        std::string c(gs.get_data(), gs.length());
        name->assign(c);
    });

    Input *input = Input::get_singleton();
    if (input->is_action_pressed("ui_accept"))
    {
        this->create_entity();
    }
}

void EntityManager::create_entity()
{
    Godot::print("Adding node");
    entt::entity entity = registry.create();
    float dx = dis(gen);
    float dz = dis(gen);
    std::stringstream message;
    message << "Generating entity with velocity dx(" << dx << ") dz(" << dz << ")";
    Godot::print(message.str().c_str());
    registry.assign<position>(entity, 0.0f, 0.0f, 0.0f);
    registry.assign<velocity>(entity, dx, 0.0f, dz);
    Spatial* entity_node = reinterpret_cast<Spatial*>(entity_scene->instance());
    this->add_child(entity_node);
    entity_node->set_script(this->entity_script.ptr());
    registry.assign<Spatial*>(entity, entity_node);
    Godot::print("Node Added");
}
