//
// Created by matteo on 9/20/19.
//

#include <iostream>
#include <sstream>
#include <cassert>
#include <functional>
#include <steam/steamnetworkingtypes.h>
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include "Server.h"

Server::Server(uint16 port)
{
    addr_server.Clear();
    addr_server.m_port = port;
    init_steam_datagram_connection_sockets();

    // Select instance to use.  For now we'll always use the default.
    // But we could use SteamGameServerNetworkingSockets() on Steam.
    network_interface = SteamNetworkingSockets();
    listen_socket = network_interface->CreateListenSocketIP(addr_server, 0, nullptr);
    if (listen_socket == k_HSteamListenSocket_Invalid)
    {
        std::ostringstream error_message;
        error_message << "Failed to listen to port " << port;
        throw std::runtime_error(error_message.str());
    }
    std::cout << "Server listening to port " << port << std::endl;


}

Server::~Server()
{}

void Server::init_steam_datagram_connection_sockets()
{
#ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
    SteamDatagramErrMsg error_message;
    if ( not GameNetworkingSockets_Init(nullptr, error_message ) )
    {
        std::cerr << "GameNetworkingSockets_Init failed. " << error_message << std::endl;
        throw std::runtime_error("GameNetworkingSockets_Init failed");
    }
#else
    SteamDatagramClient_SetAppID( 570 ); // Just set something, doesn't matter what
    //SteamDatagramClient_SetUniverse( k_EUniverseDev );

    SteamDatagramErrMsg error_message;
    if ( !SteamDatagramClient_Init( true, error_message ) )
        FatalError( "SteamDatagramClient_Init failed.  %s", error_message );

    // Disable authentication when running with Steam, for this
    // example, since we're not a real app.
    //
    // Authentication is disabled automatically in the open-source
    // version since we don't have a trusted third party to issue
    // certs.
    SteamNetworkingUtils()->SetGlobalConfigValueInt32( k_ESteamNetworkingConfig_IP_AllowWithoutAuth, 1 );
#endif

    g_logTimeZero = SteamNetworkingUtils()->GetLocalTimestamp();

    using namespace std::placeholders;
    auto lambda1 =
            std::bind(&Server::DebugOutput, this, _1, _2);
    auto lambda = [this] (ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg)
    {
        this->DebugOutput(eType, pszMsg);
    };
    auto callback = reinterpret_cast<FSteamNetworkingSocketsDebugOutput>(&lambda1);

    SteamNetworkingUtils()->SetDebugOutputFunction(
            k_ESteamNetworkingSocketsDebugOutputType_Msg,
            callback
    );
}

void Server::DebugOutput( ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg ) const
{
    SteamNetworkingMicroseconds time = SteamNetworkingUtils()->GetLocalTimestamp() - g_logTimeZero;
    std::cout << time*1e-6 << ' ' << pszMsg << std::endl;
    if ( eType == k_ESteamNetworkingSocketsDebugOutputType_Bug )
    {
        throw std::runtime_error("Well, the example dies here :)");
    }
}

void Server::run()
{
    if (listening_thread)
        throw std::runtime_error("Server already running");
    listening_thread = std::make_unique<std::thread>([this] () {threaded_run();});
}

void Server::close()
{
    thread_close = true;
    listening_thread->join();
}

void Server::threaded_run()
{
    while (not thread_close)
    {
        poll_incoming_messages();
        poll_connection_state_changes();
    }
}

void Server::poll_incoming_messages()
{
    ISteamNetworkingMessage *incoming_message = nullptr;
    int num_msg = network_interface->ReceiveMessagesOnListenSocket( listen_socket, &incoming_message, 1 );
    if (num_msg < 0) {
        throw std::runtime_error("Server error: checking for messages");
    } else if (num_msg == 0) {
        return;
    }
    assert( num_msg == 1 && incoming_message);
    auto it_client = client_map.find( incoming_message->m_conn );
    assert( it_client != client_map.end());

    std::string message;
    message.assign((const char *) incoming_message->m_pData, incoming_message->m_cbSize );
    incoming_message->Release();

    std::cout << "Received message:\"" << message << '"' << std::endl;
}

void Server::poll_connection_state_changes()
{
    network_interface->RunCallbacks(this);
}
