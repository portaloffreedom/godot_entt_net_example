//
// Created by matteo on 9/20/19.
//

#include "Client.h"
#include <iostream>
#include <cassert>
#include <utility>
#include <steam/isteamnetworkingutils.h>
#include <google/protobuf/stubs/common.h>
#include <message.pb.h>

Client::Client(std::shared_ptr<Network> net, const std::string &address, uint16 port)
        : network(std::move(net))
        , addr_server()
        , connection_thread(nullptr)
        , connection()
        , network_interface(nullptr)
{
    addr_server.Clear();
    addr_server.ParseString(address.c_str());
    addr_server.m_port = port;
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // Select instance to use. For now we'll always use the default.
    network_interface = SteamNetworkingSockets();
}

Client::~Client()
{
    if (connection_thread)
    {
        this->close();
    }
}

void Client::run()
{
    if (connection_thread)
    {
        throw std::runtime_error("Already connected");
    }

    // Start connecting
    char addr[SteamNetworkingIPAddr::k_cchMaxString];
    addr_server.ToString(addr, sizeof(addr), true);
    std::cout << "Connecting to chat server at " << addr << std::endl;
    connection = network_interface->ConnectByIPAddress(addr_server, 0, nullptr);
    if (connection == k_HSteamNetConnection_Invalid)
    {
        throw std::runtime_error("Failed to create connection");
    }

    thread_run.test_and_set();
    connection_thread = std::make_unique<std::thread>([this]() {
        threaded_run();
    });
}

void Client::close()
{
    thread_run.clear(std::memory_order_release);
    this->join();
}

void Client::join()
{
    connection_thread->join();
    connection_thread.reset();
    thread_run.clear(std::memory_order_release);
}

void Client::threaded_run()
{
    while (thread_run.test_and_set(std::memory_order_acquire))
    {
        poll_incoming_messages();
        poll_connection_state_changes();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Client::poll_incoming_messages()
{
    ISteamNetworkingMessage *incoming_message = nullptr;
    int num_messages = network_interface->ReceiveMessagesOnConnection(connection, &incoming_message, 1);
    if (num_messages < 0)
    {
        throw std::runtime_error("Error checking for messages");
    }
    if (num_messages == 0)
    {
        return;
    }
    assert(num_messages == 1 && incoming_message);
    Godot::GNSMessage message;
    message.ParseFromArray(incoming_message->m_pData, incoming_message->m_cbSize);
    switch (message.type())
    {
        case Godot::MessageType::TEXT:
            assert(message.has_text());
            std::cout << message.text().text() << std::endl;
            break;
        case Godot::MessageType::FRAME:
            std::cout << "Received Frame message ";
//            std::cout << message.DebugString();
            std::cout << message.frame().entities_size();
            std::cout << std::endl;
            {
                std::lock_guard<std::mutex> lock(last_frame_mutex);
                _last_frame = message.frame();
            }
            break;
        default:
            std::clog << "received unrecognized message of type ";
            const std::string& message_type = Godot::MessageType_Name(message.type());
            if (message_type.empty())
            {
                std::clog << '(' << message.type() << ')';
            }
            else
            {
                std::clog << message_type;
            }
            std::clog << std::endl;
            break;
    }
    incoming_message->Release();
}

void Client::poll_connection_state_changes()
{
    network_interface->RunCallbacks(this);
}

void Client::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *info)
{
    assert(info->m_hConn == connection || connection == k_HSteamNetConnection_Invalid);

    // What's the state of the connection?
    switch (info->m_info.m_eState)
    {
        case k_ESteamNetworkingConnectionState_None:
            // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
            break;

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        {
            thread_run.clear();

            // Print an appropriate message
            if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting)
            {
                // Note: we could distinguish between a timeout, a rejected connection,
                // or some other transport problem.
                printf("We sought the remote host, yet our efforts were met with defeat.  (%s)",
                       info->m_info.m_szEndDebug);
            }
            else if (info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)
            {
                printf("Alas, troubles beset us; we have lost contact with the host.  (%s)", info->m_info.m_szEndDebug);
            }
            else
            {
                // NOTE: We could check the reason code for a normal disconnection
                printf("The host hath bidden us farewell.  (%s)", info->m_info.m_szEndDebug);
            }

            // Clean up the connection.  This is important!
            // The connection is "closed" in the network sense, but
            // it has not been destroyed.  We must close it on our end, too
            // to finish up.  The reason information do not matter in this case,
            // and we cannot linger because it's already closed on the other end,
            // so we just pass 0's.
            network_interface->CloseConnection(info->m_hConn, 0, nullptr, false);
            connection = k_HSteamNetConnection_Invalid;
            break;
        }

        case k_ESteamNetworkingConnectionState_Connecting:
            // We will get this callback when we start connecting.
            // We can ignore this.
            break;

        case k_ESteamNetworkingConnectionState_Connected:
            printf("Connected to server OK");
            break;

        default:
            // Silences -Wswitch
            break;
    }
}
