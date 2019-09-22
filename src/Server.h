//
// Created by matteo on 9/20/19.
//

#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <thread>
#include <steam/steamnetworkingsockets.h>

class Server : public ISteamNetworkingSocketsCallbacks
{
public:
    Server(uint16 port);

    ~Server();

    void run();

    void close();

    void join();

private:
    static void init_steam_datagram_connection_sockets();

    static void DebugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg);

    void threaded_run();

    void poll_incoming_messages();

    void poll_connection_state_changes();

    // Callback
    void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *info) override;

    void set_client_nick( HSteamNetConnection connection, const std::string &nick);

    void send_string_to_client( HSteamNetConnection connection, const std::string &text);

    void send_string_to_all_clients( const std::string &text, HSteamNetConnection except = k_HSteamNetConnection_Invalid );

    SteamNetworkingIPAddr addr_server;

    std::atomic_flag thread_run = ATOMIC_FLAG_INIT;
    std::unique_ptr<std::thread> listening_thread;
    ISteamNetworkingSockets *network_interface;
    HSteamListenSocket listen_socket;

    struct Client_t {
        std::string nick;
    };

    std::map<HSteamNetConnection, Client_t> client_map;
};
