#include <memory>
#include <sstream>
#include <ResourceLoader.hpp>
#include <Input.hpp>
#include "EntityManager.h"
#include "message_generated.h"

using namespace godot;

void EntityManager::_register_methods()
{
    register_method("_process", &EntityManager::_process);
}

inline Vector3 vec3_from_net(const FlatGodot::Vector3 &vec)
{
    return Vector3 {vec.x(), vec.y(), vec.z()};
}

inline velocity vel_from_net(const FlatGodot::Vector3 &vec)
{
    return velocity {vec.x(), vec.y(), vec.z()};
}

EntityManager::EntityManager()
    : client(nullptr)
    , server(nullptr)
    , last_created(0)
    , server_frame_last_update(std::chrono::steady_clock::duration::zero())
    , rd()
    , gen(rd())
    , dis(-5.0, 5.0)
{
    std::shared_ptr<Network> network = std::make_shared<Network>();
    try {
        server = std::make_unique<Server>(network, 1234);
    } catch (const std::runtime_error &e) {
        server.reset();
        client = std::make_unique<Client>(std::move(network), "127.0.0.1", 1234);
    }
}

EntityManager::~EntityManager()
{
    if (server) server->close();
    if (client) client->close();
}

void EntityManager::_init()
{
    this->entity_scene = ResourceLoader::get_singleton()->load("res://models/Entity.tscn");
    this->entity_script = ResourceLoader::get_singleton()->load("res://src/Entity.gdns");
    if (server)
    {
        this->create_random_entity();
        this->server->run();
    } else {
        this->client->run();
    }
}

void EntityManager::_process(float delta)
{

    if (server)
    {
        registry.view<position, velocity, Spatial*>().each([delta](position &pos, velocity &vel, Spatial* s_entity) {
            pos.x += vel.dx * delta;
            pos.y += vel.dy * delta;
            pos.z += vel.dz * delta;
            s_entity->set_translation(Vector3(pos.x, pos.y, pos.z));
        });

        if (std::chrono::steady_clock::now() - server_frame_last_update > FRAME_UPDATE_RATE)
        {
            server_frame_last_update = std::chrono::steady_clock::now();
            for (auto &entity: registry.view<position, Spatial *>())
            {
                const float arena_size = 36.0f;
                auto &pos = registry.get<position>(entity);
                if (fabs(pos.x) > arena_size or fabs(pos.y) > arena_size)
                {
                    // messaging
                    std::unique_ptr<flatbuffers::FlatBufferBuilder> builder = std::make_unique<flatbuffers::FlatBufferBuilder>();

                    auto entity_message = create_entity_message(*builder, entity);
                    auto action_message = FlatGodot::CreateAction(*builder, FlatGodot::ActionType_DESTROY_ENTITY, entity_message);
                    auto message = FlatGodot::CreateGNSMessage(*builder, FlatGodot::MyMessage_Action, action_message.Union());
                    builder->Finish(message);
                    server->send_message(std::move(builder), k_nSteamNetworkingSend_Reliable);

                    // memory cleanup
                    registry.get<Spatial *>(entity)->queue_free();
                    registry.destroy(entity);

                    // keep always at least one entity
                    if (registry.empty())
                    {
                        create_random_entity();
                    }
                }
            }
            // network
            auto builder = std::make_unique<flatbuffers::FlatBufferBuilder>();
            std::vector<flatbuffers::Offset<FlatGodot::Entity>> entities;
            registry.view<position, velocity, uuid>().each(
                    [&entities, &builder](position &pos, velocity &vel, uuid uuid) {
                        FlatGodot::Vector3 mPos(pos.x, pos.y, pos.z);
                        FlatGodot::Vector3 mVel(vel.dx, vel.dy, vel.dz);

                        entities.emplace_back(
                                FlatGodot::CreateEntity(*builder, uuid, &mPos, &mVel));
                    });
//            std::cout << "Sending message frame with (" << message.frame().entities_size() << ") elements" << std::endl;
            auto frame = FlatGodot::CreateFrame(*builder, builder->CreateVector(entities));

            auto message = FlatGodot::CreateGNSMessage(*builder, FlatGodot::MyMessage_Frame, frame.Union());
            builder->Finish(message);
            server->send_message(std::move(builder), k_nSteamNetworkingSend_UnreliableNoDelay);
        }

        Input *input = Input::get_singleton();
        if (input->is_action_pressed("ui_accept"))
        {
            this->create_random_entity();
        }
    } else { // client
        // check remote connection action
        client->operate_actions([this] (const FlatGodot::Action &action)
        {
            switch (action.type())
            {
            case FlatGodot::ActionType_CREATE_ENTITY:
            {
                const uuid remote_uuid = action.entity()->uuid();
                const position pos = vec3_from_net(*action.entity()->position());
                const velocity vel = vel_from_net(*action.entity()->velocity());
                entt::entity ent = create_entity(remote_uuid, pos, vel);
                entt_map.emplace(remote_uuid, ent);
                break;
            }
            case FlatGodot::ActionType_DESTROY_ENTITY:
            {
                const uuid remote_uuid = action.entity()->uuid();
                try
                {
                    entt::entity entity = entt_map.at(remote_uuid);
                    Spatial *s_entity = registry.get<Spatial *>(entity);
                    s_entity->queue_free();
                    registry.destroy(entity);
                    entt_map.erase(remote_uuid);
                }
                catch (const std::out_of_range &e)
                {
                    std::clog << "Destroy entity message of entity " << remote_uuid << std::endl;
                }
                break;
            }
            default:
                std::clog << "Received Unrecognized action of type "
                          << '(' << action.type() << ')'
                          << std::endl;
            }
        });


        // update using last frame
        const FlatGodot::Frame &frame = client->last_frame();
        for (const auto &remote_entity: *frame.entities()) {
            const uuid remote_uuid = remote_entity->uuid();
            const Vector3 remote_pos = vec3_from_net(*remote_entity->position());
            const velocity remote_vel = vel_from_net(*remote_entity->velocity());
            try
            {
                entt::entity local_entity = entt_map.at(remote_uuid);
                registry.replace<position>(local_entity, remote_pos);
                registry.replace<velocity>(local_entity, remote_vel);
            }
            catch (const std::out_of_range &e)
            {
                // This code creates problem because frame and action messages can arrive in the
                //  wrong order.
                //create_entity(
                //        remote_uuid,
                //        remote_pos,
                //        velocity{0.0f,0.0f,0.0f}
                //);
            }
        }

        registry.view<position, velocity, Spatial*>().each([delta](position &pos, velocity &vel, Spatial* s_entity) {
            pos.x += vel.dx * delta;
            pos.y += vel.dy * delta;
            pos.z += vel.dz * delta;
            s_entity->set_translation(Vector3(pos.x, pos.y, pos.z));
        });
    }
}

entt::entity EntityManager::create_entity(const uuid ent_uuid, const position &pos, const velocity &vel)
{
    Godot::print("Adding node");
    entt::entity entity = registry.create();
    registry.assign<uuid>(entity, ent_uuid);
    registry.assign<position>(entity, pos);
    registry.assign<velocity>(entity, vel);
    std::stringstream message;
    message << "Generating entity with velocity dx(" << vel.dx << ") dz(" << vel.dz << ")";
    Godot::print(message.str().c_str());
    Spatial* entity_node = reinterpret_cast<Spatial*>(entity_scene->instance());
    this->add_child(entity_node);
    entity_node->set_script(this->entity_script.ptr());
    registry.assign<Spatial*>(entity, entity_node);
    Godot::print("Node Added");
    return entity;
}

void EntityManager::create_random_entity()
{
    float dx = dis(gen);
    float dz = dis(gen);
    entt::entity entity = create_entity(++last_created, position{0.0f, 0.0f, 0.0f}, velocity{dx, 0.0f, dz});

    if (server)
    {
        auto builder = std::make_unique<flatbuffers::FlatBufferBuilder>();
        auto entity_message = create_entity_message(*builder, entity);
        auto action_message = FlatGodot::CreateAction(*builder, FlatGodot::ActionType_CREATE_ENTITY, entity_message);
        auto message = FlatGodot::CreateGNSMessage(*builder, FlatGodot::MyMessage_Action, action_message.Union());
        builder->Finish(message);
        server->send_message(std::move(builder), k_nSteamNetworkingSend_Reliable);
    }
}

flatbuffers::Offset<FlatGodot::Entity> EntityManager::create_entity_message(flatbuffers::FlatBufferBuilder &builder, entt::entity entity)
{
    const Vector3 &pos = registry.get<position>(entity);
    const velocity &vel = registry.get<velocity>(entity);
    const uuid ent_uuid = registry.get<uuid>(entity);

    FlatGodot::Vector3 mPos(pos.x, pos.y, pos.z);
    FlatGodot::Vector3 mVel(vel.dx, vel.dy, vel.dz);

    return FlatGodot::CreateEntity(builder, ent_uuid, &mPos, &mVel);
}
