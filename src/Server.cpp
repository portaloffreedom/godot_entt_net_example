//
// Created by matteo on 9/20/19.
//

#include <iostream>
#include <sstream>
#include <cassert>
#include <utility>
#include <steam/steamnetworkingtypes.h>
#include <steam/steamnetworkingsockets.h>
#include <text_message_generated.h>
#include <message_generated.h>
#include "Server.h"
#include "Network.h"

Server::Server(std::shared_ptr<Network> net, uint16 port)
        : network(std::move(net))
        , addr_server()
        , listening_thread(nullptr)
        , network_interface(nullptr)
        , listen_socket()
{
    addr_server.Clear();
    addr_server.m_port = port;

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
    unsigned int counter = 0;

    while (thread_run.test_and_set(std::memory_order_acquire))
    {
        poll_incoming_messages();
        poll_connection_state_changes();
        if (counter % 100 == 0)
        {
            //NOTE HERE: this ping is only for test purposes. The GameNetworkingSockets library
            // already includes a keep alive messaging system.
            std::cout << "SENDING PING" << std::endl;
            std::ostringstream message;
            message << "ping(" << counter << ')';
            send_string_to_all_clients(message.str());
        }
        poll_sending_message_queue();

        counter++;
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

void Server::send_data_to_client(HSteamNetConnection connection, const void *data, unsigned int data_size, int send_flags)
{
    network_interface->SendMessageToConnection(connection, data, data_size, send_flags);
}

void Server::send_data_to_all_clients(const void *data, unsigned int data_size, int send_flag, HSteamNetConnection except)
{
    for (auto &client: client_map)
    {
        if (client.first != except)
        {
            send_data_to_client(client.first, data, data_size, send_flag);
        }
    }
}

flatbuffers::FlatBufferBuilder serialize_text_message(const std::string &text)
{
    // Create buffer
    flatbuffers::FlatBufferBuilder builder;
    auto flat_text = builder.CreateString(text);
    auto text_message = FlatGodot::CreateTextMessage(builder, flat_text);
    auto gns_message = FlatGodot::CreateGNSMessage(builder, FlatGodot::MyMessage_TextMessage, text_message.Union());

    // Two alternative version to finish the message
    //FlatGodot::FinishGNSMessageBuffer(builder, gns_message);
    builder.Finish(gns_message);

    // Buffer and size can be accessed after builder is finished.
    //uint8_t *buf = builder.GetBufferPointer();
    //size_t size = builder.GetSize();
    return builder;
}

void Server::send_string_to_client(HSteamNetConnection connection, const std::string &text)
{
    flatbuffers::FlatBufferBuilder serialized_message = serialize_text_message(text);
    send_data_to_client(connection, serialized_message.GetBufferPointer(), serialized_message.GetSize());
}

void Server::send_string_to_all_clients(const std::string &text, HSteamNetConnection except)
{
    flatbuffers::FlatBufferBuilder serialized_message = serialize_text_message(text);
    send_data_to_all_clients(serialized_message.GetBufferPointer(), serialized_message.GetSize());
}

void Server::send_message(std::unique_ptr<flatbuffers::FlatBufferBuilder> message_buffer, int send_flag)
{
    std::lock_guard<std::mutex> lock(message_queue_mutex);
    message_queue.emplace(std::move(message_buffer), send_flag);
}

void Server::poll_sending_message_queue()
{
    std::lock_guard<std::mutex> lock(message_queue_mutex);
    while (not message_queue.empty())
    {
        const flatbuffers::FlatBufferBuilder &message_buffer = *message_queue.front().first;
        const int send_flag = message_queue.front().second;
//        std::cout << "Sending peding message (" << Godot::MessageType_Name(message.type()) << ')' << std::endl;
        send_data_to_all_clients(message_buffer.GetBufferPointer(), message_buffer.GetSize(), send_flag);
        message_queue.pop();
    }
}
