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

SteamNetworkingMicroseconds g_logTimeZero;

Server::Server(uint16 port)
        : addr_server()
        , listening_thread(nullptr)
        , listen_socket()
        , network_interface(nullptr)
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
{
    if (listening_thread)
    {
        this->close();
    }
}

void Server::init_steam_datagram_connection_sockets()
{
#ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
    SteamDatagramErrMsg error_message;
    if (not GameNetworkingSockets_Init(nullptr, error_message))
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

    auto lambda = [](ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg) {
        Server::DebugOutput(eType, pszMsg);
    };

    SteamNetworkingUtils()->SetDebugOutputFunction(
            k_ESteamNetworkingSocketsDebugOutputType_Msg,
            lambda
    );
}

void Server::DebugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg)
{
    SteamNetworkingMicroseconds time = SteamNetworkingUtils()->GetLocalTimestamp() - g_logTimeZero;
    std::cout << time * 1e-6 << ' ' << pszMsg << std::endl;
    if (eType == k_ESteamNetworkingSocketsDebugOutputType_Bug)
    {
        throw std::runtime_error("Well, the example dies here :)");
    }
}

void Server::run()
{
    if (listening_thread)
    {
        throw std::runtime_error("Server already running");
    }

    thread_run.test_and_set();
    listening_thread = std::make_unique<std::thread>([this]() {
        threaded_run();
    });
}

void Server::close()
{
    thread_run.clear(std::memory_order_release);
    this->join();
}

void Server::join()
{
    listening_thread->join();
    listening_thread.reset();
    thread_run.clear(std::memory_order_release);
}

void Server::threaded_run()
{
    while (thread_run.test_and_set(std::memory_order_acquire))
    {
        poll_incoming_messages();
        poll_connection_state_changes();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (const auto &client: client_map)
    {
        network_interface->CloseConnection(client.first, 0, "Server Shutdown", true);
    }
    client_map.clear();
}

void Server::poll_incoming_messages()
{
    ISteamNetworkingMessage *incoming_message = nullptr;
    int num_msg = network_interface->ReceiveMessagesOnListenSocket(listen_socket, &incoming_message, 1);
    if (num_msg < 0)
    {
        throw std::runtime_error("Server error: checking for messages");
    }
    else if (num_msg == 0)
    {
        return;
    }
    assert(num_msg == 1 && incoming_message);
    auto it_client = client_map.find(incoming_message->m_conn);
    assert(it_client != client_map.end());

    std::string message;
    message.assign((const char *) incoming_message->m_pData, incoming_message->m_cbSize);
    incoming_message->Release();

    std::cout << "Received message:\"" << message << '"' << std::endl;
}

void Server::poll_connection_state_changes()
{
    network_interface->RunCallbacks(this);
}

void Server::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *info)
{
    std::cout << "Server::" << __func__ << '(' << info << ')' << std::endl;
    std::cout << " connection handle: " << info->m_hConn << std::endl;
    std::cout << " connection state: " << info->m_info.m_eState << std::endl;
    std::cout << " connection old state: " << info->m_eOldState << std::endl;
    char temp[1024];

    // What's the state of the connection?
    switch (info->m_info.m_eState)
    {
        case k_ESteamNetworkingConnectionState_None:
            // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
            break;

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        {
            // Ignore if they were not previously connected.  (If they disconnected
            // before we accepted the connection.)
            if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connected)
            {

                // Locate the client.  Note that it should have been found, because this
                // is the only codepath where we remove clients (except on shutdown),
                // and connection change callbacks are dispatched in queue order.
                auto itClient = client_map.find(info->m_hConn);
                assert(itClient != client_map.end());

                // Select appropriate log messages
                const char *pszDebugLogAction;
                if (info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)
                {
                    pszDebugLogAction = "problem detected locally";
                    sprintf(temp, "Alas, %s hath fallen into shadow.  (%s)", itClient->second.nick.c_str(),
                            info->m_info.m_szEndDebug);
                }
                else
                {
                    // Note that here we could check the reason code to see if
                    // it was a "usual" connection or an "unusual" one.
                    pszDebugLogAction = "closed by peer";
                    sprintf(temp, "%s hath departed", itClient->second.nick.c_str());
                }

                // Spew something to our own log.  Note that because we put their nick
                // as the connection description, it will show up, along with their
                // transport-specific data (e.g. their IP address)
                std::cout << "Connection " << info->m_info.m_szConnectionDescription
                          << ' ' << pszDebugLogAction
                          << ", reason " << info->m_info.m_eEndReason
                          << ": " << info->m_info.m_szEndDebug
                          << std::endl;

                client_map.erase(itClient);

                // Send a message so everybody else knows what happened
                send_string_to_all_clients(temp);
            }
            else
            {
                assert(info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting);
            }

            // Clean up the connection.  This is important!
            // The connection is "closed" in the network sense, but
            // it has not been destroyed.  We must close it on our end, too
            // to finish up.  The reason information do not matter in this case,
            // and we cannot linger because it's already closed on the other end,
            // so we just pass 0's.
            network_interface->CloseConnection(info->m_hConn, 0, nullptr, false);
            break;
        }

        case k_ESteamNetworkingConnectionState_Connecting:
        {
            // This must be a new connection
            assert(client_map.find(info->m_hConn) == client_map.end());

            std::cout << "Connection request from " << info->m_info.m_szConnectionDescription << std::endl;

            // A client is attempting to connect
            // Try to accept the connection.
            if (network_interface->AcceptConnection(info->m_hConn) != k_EResultOK)
            {
                // This could fail.  If the remote host tried to connect, but then
                // disconnected, the connection may already be half closed.  Just
                // destroy whatever we have on our side.
                network_interface->CloseConnection(info->m_hConn, 0, nullptr, false);
                std::cout << "Can't accept connection.  (It was already closed?)" << std::endl;
                break;
            }

            // Generate a random nick.  A random temporary nick
            // is really dumb and not how you would write a real chat server.
            // You would want them to have some sort of signon message,
            // and you would keep their client in a state of limbo (connected,
            // but not logged on) until them.  I'm trying to keep this example
            // code really simple.
            char nick[64];
            sprintf(nick, "BraveWarrior%d", 10000 + (rand() % 100000));

            // Send them a welcome message
            sprintf(temp,
                    "Welcome, stranger.  Thou art known to us for now as '%s'; upon thine command '/nick' we shall know thee otherwise.",
                    nick);
            send_string_to_client(info->m_hConn, temp);

            // Also send them a list of everybody who is already connected
            if (client_map.empty())
            {
                send_string_to_client(info->m_hConn, "Thou art utterly alone.");
            }
            else
            {
                sprintf(temp, "%d companions greet you:", (int) client_map.size());
                for (auto &client: client_map)
                    send_string_to_client(info->m_hConn, client.second.nick.c_str());
            }

            // Let everybody else know who they are for now
            sprintf(temp, "Hark!  A stranger hath joined this merry host.  For now we shall call them '%s'", nick);
            send_string_to_all_clients(temp, info->m_hConn);

            // Add them to the client list, using std::map wacky syntax
            client_map[info->m_hConn];
            set_client_nick(info->m_hConn, nick);
            break;
        }

        case k_ESteamNetworkingConnectionState_Connected:
            // We will get a callback immediately after accepting the connection.
            // Since we are the server, we can ignore this, it's not news to us.
            break;

        default:
            // Silences -Wswitch
            break;
    }
}


void Server::set_client_nick(HSteamNetConnection connection, const std::string &nick)
{
    client_map[connection].nick = nick;

    network_interface->SetConnectionName(connection, nick.c_str());
}

void Server::send_string_to_client(HSteamNetConnection connection, const std::string &text)
{
    network_interface->SendMessageToConnection(connection, text.c_str(), text.size(), k_nSteamNetworkingSend_Reliable);
}

void Server::send_string_to_all_clients(const std::string &text, HSteamNetConnection except)
{
    for (auto &client: client_map)
    {
        if (client.first != except)
        {
            send_string_to_client(client.first, text);
        }
    }
}
