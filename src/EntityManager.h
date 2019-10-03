#pragma once

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
    void create_entity_message(::Godot::Entity *message, entt::entity entity);

    // registry
    entt::registry registry;
    uuid last_created;

    // preload of the resources to create a single entity
    Ref<PackedScene> entity_scene;
    Ref<Resource> entity_script;

    // stuff for the random generator
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<float> dis;

    // Server class
    std::unique_ptr<Server> server;
    std::unique_ptr<Client> client;
};

}
