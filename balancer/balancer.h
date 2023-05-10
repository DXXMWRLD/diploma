#pragma once

#include <steam/steamnetworkingsockets.h>
#include <unordered_map>
#include "statistic.h"
#include <nlohmann/json.hpp>

class Balancer {
public:
  Balancer(uint16_t port);

public:
  static void netConnectionStatusChangeCallBack(SteamNetConnectionStatusChangedCallback_t* info);
  void pollConnectionStateChanges();
  void onSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);
  void processIncomingMessages();
  void netThreadRunFunc();
  void addConnectionToPollGroup(HSteamNetConnection conn) const;
  bool receiveMessage();
  void run();


  nlohmann::json serverDistribution();

public:
  HSteamListenSocket listenSocket_;
  SteamNetworkingIPAddr serverAddr_{};
  uint16_t port_ = 0;
  static Balancer* callbackInstance_;
  bool serverIsRunning_ = false;
  HSteamNetPollGroup pollGroup_;

  std::unordered_map<HSteamNetConnection, Statistic> servers_;
};