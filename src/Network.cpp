//
// Created by matteo on 9/29/19.
//

#include <iostream>
#include <steam/steamnetworkingtypes.h>
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include "Network.h"

Network::Network()
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

//    g_logTimeZero = SteamNetworkingUtils()->GetLocalTimestamp();

    auto lambda = [](ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg) {
        std::cerr << pszMsg << std::endl;
        if (eType == k_ESteamNetworkingSocketsDebugOutputType_Bug)
        {
            throw std::runtime_error("Well, the example dies here :)");
        }
    };

    SteamNetworkingUtils()->SetDebugOutputFunction(
            k_ESteamNetworkingSocketsDebugOutputType_Msg,
            lambda
    );
}

Network::~Network()
{
    GameNetworkingSockets_Kill();
}
