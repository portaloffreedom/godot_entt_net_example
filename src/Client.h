//
// Created by matteo on 9/20/19.
//

#ifndef ENTT_EXAMPLE_CLIENT_H
#define ENTT_EXAMPLE_CLIENT_H

#include <atomic>
#include <memory>
#include <thread>
#include <steam/steamnetworkingsockets.h>

class Client : ISteamNetworkingSocketsCallbacks
{
public:
    Client(const std::string &address, uint16 port);

    ~Client();

    void run();

    void close();

    void join();

private:
    static void init_steam_datagram_connection_sockets();

    void threaded_run();

    void poll_incoming_messages();

    void poll_connection_state_changes();

    // callback
    void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *callback) override;

    SteamNetworkingIPAddr addr_server;

    std::atomic_flag thread_run = ATOMIC_FLAG_INIT;
    std::unique_ptr<std::thread> connection_thread;
    ISteamNetworkingSockets *network_interface;
    HSteamNetConnection connection;
};


#endif //ENTT_EXAMPLE_CLIENT_H
