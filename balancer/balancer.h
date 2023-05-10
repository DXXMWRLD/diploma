#pragma once

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <steam/steam_api.h>
#endif
#include <unordered_map>
#include "statistic.h"
#include <nlohmann/json.hpp>
#include <thread>
#include "server/server.h"


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
  void startNewServer(nlohmann::json);

  nlohmann::json serverDistribution();

public:
  HSteamListenSocket listenSocket_;
  SteamNetworkingIPAddr serverAddr_{};
  uint16_t port_ = 0;
  static Balancer* callbackInstance_;
  bool serverIsRunning_ = false;
  HSteamNetPollGroup pollGroup_;

  std::vector<std::thread> threads_;
  std::unordered_map<HSteamNetConnection, std::unique_ptr<Server>> servers_;
  std::unordered_map<HSteamNetConnection, int32_t> connectionToPort_;
  std::unordered_map<nlohmann::json, std::pair<HSteamNetConnection, Statistic>> statistics_;

  int currentPort_ = 6655;
};