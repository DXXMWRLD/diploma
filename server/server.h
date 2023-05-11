#pragma once

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <steam/steam_api.h>
#endif
#include <unordered_map>
#include <thread>
#include <atomic>
#include <iostream>
#include <set>
#include <nlohmann/json.hpp>

// class World {
// public:
//   World() = default;
//   World(int wuid)
//       : id_(wuid) {
//   }
//   ~World() {
//     stop();
//   }

// public:
//   void coreLoop() {
//     while (isRunning_) {
//       std::this_thread::sleep_for(std::chrono::milliseconds(50));
//     }
//   }

//   void run() {
//     isRunning_ = true;
//     std::cout << "Starting world core loop" << std::endl;
//     thread_ = std::thread(&World::coreLoop, this);
//   }

//   void stop() {
//     std::cout << "Stopping world core loop" << std::endl;
//     isRunning_ = false;
//     thread_.join();
//   }

// public:
//   int id_;
//   std::atomic<bool> isRunning_ = false;
//   std::thread thread_;
// };


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
  bool receiveMessage();
  void run();
  void connectBalancer();

  int32_t worldDistribution();
  void addClientToWorld(int32_t world_id);
  void runWorld(int32_t world_id);
  nlohmann::json generateStatisticsMessage(float cpu_load);

public:
  HSteamListenSocket listenSocket_;
  SteamNetworkingIPAddr serverAddr_{};
  uint16_t port_ = 0;
  static Server* callbackInstance_;
  bool serverIsRunning_ = false;
  HSteamNetPollGroup pollGroup_;

  struct ClientInfo {
    ClientInfo() = default;
    ClientInfo(int32_t world_id)
        : worldId_(world_id) {
    }
    ~ClientInfo() = default;
    int32_t worldId_;
  };

  std::set<int32_t> worlds_;
  std::unordered_map<HSteamNetConnection, ClientInfo> clients_;
  std::thread netThread_;

  bool isBalancerConnected_ = false;
  HSteamNetPollGroup balancerPollGroup_;
  HSteamNetConnection balancerConnection_{};
};