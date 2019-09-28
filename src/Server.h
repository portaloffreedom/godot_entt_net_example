//
// Created by matteo on 9/20/19.
//

#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <steam/steamnetworkingsockets.h>
#include "message.pb.h"
#include "Network.h"


class Server : public ISteamNetworkingSocketsCallbacks
{
public:
    Server(std::shared_ptr<Network>, uint16 port);

    ~Server();

    void run();

    void close();

    void join();

    void send_message(const Godot::GNSMessage &message);

private:
    void threaded_run();

    void poll_incoming_messages();

    void poll_connection_state_changes();

    void poll_sending_message_queue();

    // Callback
    void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *info) override;

    void set_client_nick( HSteamNetConnection connection, const std::string &nick);

    void send_data_to_client( HSteamNetConnection connection, const void* data, unsigned int data_size );

    void send_data_to_all_clients( const void* data, unsigned int data_size, HSteamNetConnection except = k_HSteamNetConnection_Invalid );

    void send_string_to_client( HSteamNetConnection connection, const std::string &text);

    void send_string_to_all_clients( const std::string &text, HSteamNetConnection except = k_HSteamNetConnection_Invalid );

    std::shared_ptr<Network> network;
    SteamNetworkingIPAddr addr_server;

    std::atomic_flag thread_run = ATOMIC_FLAG_INIT;
    std::unique_ptr<std::thread> listening_thread;
    ISteamNetworkingSockets *network_interface;
    HSteamListenSocket listen_socket;

    struct Client_t {
        std::string nick;
    };

    std::map<HSteamNetConnection, Client_t> client_map;

    std::mutex message_queue_mutex;
    std::queue<Godot::GNSMessage> message_queue;
};
