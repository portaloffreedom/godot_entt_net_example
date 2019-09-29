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

    void send_message(const Godot::GNSMessage &message, int send_flag = k_nSteamNetworkingSend_Reliable);

private:
    void threaded_run();

    void poll_incoming_messages();

    void poll_connection_state_changes();

    void poll_sending_message_queue();

    // Callback
    void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *info) override;

    void set_client_nick( HSteamNetConnection connection, const std::string &nick);

    void send_data_to_client( HSteamNetConnection connection, const void* data, unsigned int data_size, int send_flag = k_nSteamNetworkingSend_Reliable);

    void send_data_to_all_clients( const void* data, unsigned int data_size, int send_flag = k_nSteamNetworkingSend_Reliable, HSteamNetConnection except = k_HSteamNetConnection_Invalid);

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
    std::queue<std::pair<Godot::GNSMessage, int>> message_queue;
};
