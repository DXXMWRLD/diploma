#pragma once

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <steam/steam_api.h>
#endif
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <thread>
#include "server/server.h"


struct ServerInfo {
  ServerInfo()  = default;
  ~ServerInfo() = default;

  void print() const {
    std::cout << "\tServer Info: " << std::endl;
    std::cout << "\t\tAddress: " << address << std::endl;
    std::cout << "\t\tPort: " << port << std::endl;
    std::cout << "\t\tWorlds count: " << worlds_count << std::endl;
    std::cout << "\t\tPlayers count: " << players_count << std::endl;
  }

  std::string address;
  uint16_t port;
  int worlds_count;
  int players_count;
};


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
  void print() const {
    std::cout << "Server Cluster Info: " << std::endl;
    for (auto& [conn, statistic] : statistic_) {
      std::cout << "\tConnection: " << conn << std::endl;
      statistic.print();
      std::cout << std::endl;
    }
  }

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
  std::unordered_map<HSteamNetConnection, ServerInfo> statistic_;

  // std::unordered_map<HSteamNetConnection, int32_t> connectionToPort_;

  int currentPort_ = 6655;
};