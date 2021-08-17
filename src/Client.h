//
// Created by matteo on 9/20/19.
//

#ifndef ENTT_EXAMPLE_CLIENT_H
#define ENTT_EXAMPLE_CLIENT_H

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <steam/steamnetworkingsockets.h>
#include <frame_generated.h>
#include <message_generated.h>
#include <deque>
#include "Network.h"
#include "MessageContainer.h"

class Client : ISteamNetworkingSocketsCallbacks
{
public:
    Client(std::shared_ptr<Network>, const std::string &address, uint16 port);

    ~Client();

    void run();

    void close();

    void join();

    const FlatGodot::Frame &last_frame()
    {
        std::lock_guard<std::mutex> lock (last_frame_mutex);
        return this->_last_frame.Data();
    }

    unsigned int operate_actions(const std::function<void(const FlatGodot::Action &)>& fun);

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
    MessageContainer<FlatGodot::Frame> _last_frame;

    std::mutex pending_actions_mutex;
    std::deque<MessageContainer<FlatGodot::Action>> _pending_actions;
};


#endif //ENTT_EXAMPLE_CLIENT_H
