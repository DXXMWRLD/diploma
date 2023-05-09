#pragma once

#include <thread>
#include <atomic>
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <steam/steam_api.h>
#endif

class ClientConnectionHandler {
public:
  ClientConnectionHandler(const std::string& address, uint16_t port);
  ~ClientConnectionHandler();

public:
  void init(const std::string& address, uint16_t port);
  void run();
  void close();
  void pollConnectionStateChanges();

public:
  HSteamNetConnection steamConnection_{};

private:
  void setNewServerAddress(SteamNetworkingIPAddr&& new_addr);
  void onSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo);
  static ClientConnectionHandler* sPCallbackInstance_;
  static void steamNetConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* pInfo) {
    sPCallbackInstance_->onSteamNetConnectionStatusChanged(pInfo);
  }

public:
  HSteamNetPollGroup pollGroup_;
  SteamNetworkingIPAddr addrServer_;
  std::atomic<bool> connectionIsRun_{};
};
