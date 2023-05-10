#include <iostream>
#include <cstdio>
#include <cstdarg>
#include <unistd.h>
#include <csignal>
#include "client.h"
#include <nlohmann/json.hpp>
#include "steam/steamnetworkingsockets.h"
#include "steam/isteamnetworkingutils.h"
#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <steam/steam_api.h>
#endif


using namespace std;
using json = nlohmann::json;


static void NukeProcess(int rc) {
#ifdef WIN32
  ExitProcess(rc);
#else
  (void)rc; // Unused formal parameter
  kill(getpid(), SIGKILL);
#endif
}

static void DebugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char* pszMsg) {
  SteamNetworkingMicroseconds time = 0;
  printf("%10.6f %s\n", time * 1e-6, pszMsg);
  fflush(stdout);
  if (eType == k_ESteamNetworkingSocketsDebugOutputType_Bug) {
    fflush(stdout);
    fflush(stderr);
    NukeProcess(1);
  }
}

static void FatalError(const char* fmt, ...) {
  char text[2048];
  va_list ap;
  va_start(ap, fmt);
  vsprintf(text, fmt, ap);
  va_end(ap);
  char* nl = strchr(text, '\0') - 1;
  if (nl >= text && *nl == '\n')
    *nl = '\0';
  DebugOutput(k_ESteamNetworkingSocketsDebugOutputType_Bug, text);
}

static void InitSteamDatagramConnectionSockets() {
#ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
  SteamDatagramErrMsg errMsg;
  if (!GameNetworkingSockets_Init(nullptr, errMsg))
    FatalError("GameNetworkingSockets_Init failed.  %s", errMsg);
#else
  SteamDatagramClient_SetAppID(570); // Just set something, doesn't matter what
  // SteamDatagramClient_SetUniverse( k_EUniverseDev );

  SteamDatagramErrMsg errMsg;
  if (!SteamDatagramClient_Init(true, errMsg))
    FatalError("SteamDatagramClient_Init failed.  %s", errMsg);

  // Disable authentication when running with Steam, for this
  // example, since we're not a real app.
  //
  // Authentication is disabled automatically in the open-source
  // version since we don't have a trusted third party to issue
  // certs.
  SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_IP_AllowWithoutAuth, 1);
#endif

  // g_logTimeZero = SteamNetworkingUtils()->GetLocalTimestamp();

  SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, DebugOutput);
}


int main(int argc, char const* argv[]) {

  if (argc != 3) {
    cerr << "Usage: " << argv[0] << " <port>" << endl;
    return 1;
  }

  auto address = argv[1];
  auto port    = atoi(argv[2]);

  InitSteamDatagramConnectionSockets();

  // // create a client socket
  Client client(address, port);

  // // send a json message
  json message       = {{"name", "Alice"}, {"age", 30}};
  string message_str = message.dump();


  int bytes_sent
      = SteamNetworkingSockets()->SendMessageToConnection(client.connection_.steamConnection_, message_str.c_str(),
                                                          message_str.size(), k_nSteamNetworkingSend_Reliable, nullptr);
  if (bytes_sent < 0) {
    cerr << "Error: failed to send message." << endl;
    return 1;
  }

  while (true) {
    client.netThreadRunFunc();
  }

  return 0;
}