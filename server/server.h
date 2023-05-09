#pragma once

#include <steam/steamnetworkingsockets.h>
#include <unordered_map>


class Server {
public:
  Server(uint16_t port);

public:
  static void netConnectionStatusChangeCallBack(SteamNetConnectionStatusChangedCallback_t* info);
  void pollConnectionStateChanges();
  void onSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);
  void processIncomingMessages();
  void netThreadRunFunc();
  void addConnectionToPollGroup(HSteamNetConnection conn) const;

public:
  HSteamListenSocket listenSocket_;
  SteamNetworkingIPAddr serverAddr_{};
  uint16_t port_ = 0;
  static Server* callbackInstance_;
  bool serverIsRunning_ = false;
  HSteamNetPollGroup pollGroup_;
  struct ClientInfo {
    ClientInfo(int32_t world_id)
        : worldId_(world_id) {
    }

    int32_t worldId_;
  };
  std::unordered_map<HSteamNetConnection, ClientInfo> clients_;
};