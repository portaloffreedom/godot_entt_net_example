#pragma once

#include <chrono>
#include <random>
#include <Godot.hpp>
#include <Spatial.hpp>
#include <PackedScene.hpp>
#include <entt/entt.hpp>
#include "Server.h"
#include "Client.h"


namespace godot {

typedef Vector3 position;
typedef unsigned int uuid;

struct velocity
{
    float dx;
    float dy;
    float dz;
};

class EntityManager : public Spatial {
    GODOT_CLASS(EntityManager, Spatial)

public:
    static void _register_methods();

    EntityManager();
    virtual ~EntityManager();

    void _init();
    void _process(float delta);

    entt::entity create_entity(uuid ent_uuid, const position &pos, const velocity &vel);
    void create_random_entity();
private:
    flatbuffers::Offset<FlatGodot::Entity> create_entity_message(flatbuffers::FlatBufferBuilder &builder, entt::entity entity);

    // preload of the resources to create a single entity
    Ref<PackedScene> entity_scene;
    Ref<Resource> entity_script;

    // registry
    entt::registry registry;

    // Client stuff --------------------
    std::unique_ptr<Client> client;
    std::map<uuid, entt::entity> entt_map;

    // Server stuff --------------------
    std::unique_ptr<Server> server;
    uuid last_created;
    const std::chrono::steady_clock::duration FRAME_UPDATE_RATE = std::chrono::milliseconds(50);
    std::chrono::steady_clock::time_point server_frame_last_update;

    // stuff for the random generator
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<float> dis;

};

}
