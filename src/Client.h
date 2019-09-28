//
// Created by matteo on 9/20/19.
//

#ifndef ENTT_EXAMPLE_CLIENT_H
#define ENTT_EXAMPLE_CLIENT_H

#include <atomic>
#include <memory>
#include <thread>
#include <steam/steamnetworkingsockets.h>
#include <frame.pb.h>
#include "Network.h"

class Client : ISteamNetworkingSocketsCallbacks
{
public:
    Client(std::shared_ptr<Network>, const std::string &address, uint16 port);

    ~Client();

    void run();

    void close();

    void join();

    Godot::Frame last_frame()
    {
        std::lock_guard<std::mutex> lock (last_frame_mutex);
        return this->_last_frame;
    }

private:
    void threaded_run();

    void poll_incoming_messages();

    void poll_connection_state_changes();

    // callback
    void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *callback) override;

    std::shared_ptr<Network> network;
    SteamNetworkingIPAddr addr_server;

    std::atomic_flag thread_run = ATOMIC_FLAG_INIT;
    std::unique_ptr<std::thread> connection_thread;
    ISteamNetworkingSockets *network_interface;
    HSteamNetConnection connection;

    std::mutex last_frame_mutex;
    Godot::Frame _last_frame;
};


#endif //ENTT_EXAMPLE_CLIENT_H
