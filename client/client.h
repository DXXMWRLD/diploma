#pragma once

#include <steam/steamnetworkingsockets.h>
#include <string>
#include "connection.h"

class Client {
public:
  Client(const std::string& address, uint16_t port);

public:
  void pollConnectionStateChanges();
  void connectToRealm();
  void netThreadRunFunc();
  void processIncomingMessages();
  bool receiveMessage();

public:
  SteamNetworkingIPAddr addrServer_;
  HSteamNetConnection steamConnection_{};
  ClientConnectionHandler connection_;
  bool isRunning_            = true;
  bool isConnectedToBalancer = false;
};