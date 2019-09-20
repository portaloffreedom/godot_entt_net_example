//
// Created by matteo on 9/20/19.
//

#pragma once

#include <memory>
#include <thread>
#include <map>

class Server : private ISteamNetworkingSocketsCallbacks
{
public:
    Server(uint16 port);

    ~Server();

    void run();

    void close();

private:
    void init_steam_datagram_connection_sockets();

    void DebugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg) const;

    void threaded_run();

    void poll_incoming_messages();

    void poll_connection_state_changes();

    SteamNetworkingIPAddr addr_server;
    SteamNetworkingMicroseconds g_logTimeZero;

    bool thread_close;
    std::unique_ptr<std::thread> listening_thread;
    HSteamListenSocket listen_socket;
    ISteamNetworkingSockets *network_interface;

    struct Client_t {
        std::string nick;
    };

    std::map<HSteamNetConnection, Client_t> client_map;
};
