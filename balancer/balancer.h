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
#include "sample/settings.h"
#include "sample/colors.h"


enum class State { Decrease = 0, Stability = 1, Increase = 2 };


struct ServerInfo {
  ServerInfo()  = default;
  ~ServerInfo() = default;

  void print() const {
    std::cout << GRN "\tServer Info: " << std::endl;
    std::cout << GRN "\t\tAddress: " << address << std::endl;
    std::cout << GRN "\t\tPort: " << port << std::endl;
    std::cout << GRN "\t\tWorlds count: " << worlds_count << std::endl;
    std::cout << GRN "\t\tPlayers count: " << players_count << std::endl;
    std::cout << GRN "\t\tPending players count: " << f_players_count << std::endl;
    float load = (WORLD_WEIGHT * (worlds_count + f_worlds_count) + CLIENT_WEIGHT * (players_count + f_players_count))
                 / (SERVER_WEIGHT * MAX_CPU_LOAD / 100.f);
    std::cout << GRN "\t\tServer load = " << load << "%" << std::endl;
  }

  std::string address;
  uint16_t port;
  int worlds_count;
  int players_count;

  int f_worlds_count  = 0;
  int f_players_count = 0;
};


class StateMachine {
public:
  StateMachine()  = default;
  ~StateMachine() = default;

  void process(const std::unordered_map<HSteamNetConnection, ServerInfo>& statistic);

  State getState() const;

public:
  State state = State::Increase;
  std::vector<float> cpu_load;
  int i = -1;
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
    std::cout << GRN "\nServer Cluster Info: " << std::endl;
    for (auto& [conn, statistic] : statistic_) {
      std::cout << GRN "\tConnection: " << conn << std::endl;
      statistic.print();
      std::cout << std::endl;
    }
  }

  void processStateMachine() {
    stateMachine_.process(statistic_);
  }

  nlohmann::json serverDistribution();
  int32_t findLowServer() {
    int i   = 10000;
    int con = -1;
    for (auto& [conn, statistic] : statistic_) {
      if (statistic.players_count < i
          && ((WORLD_WEIGHT * (statistic.worlds_count + 1 + statistic.f_worlds_count)
               + CLIENT_WEIGHT * (statistic.players_count + 1 + statistic.f_players_count))
              < SERVER_WEIGHT * MAX_CPU_LOAD)) {
        i   = statistic.players_count;
        con = conn;
      }
    }
    // if (con == -1) {
    //   return currentPort_++;
    // }

    auto& stat = statistic_[con];
    ++stat.f_worlds_count;
    ++stat.f_players_count;
    return stat.port;
  }

  int32_t findHighServer() {
    int i   = -1000;
    int con = -1;
    for (auto& [conn, statistic] : statistic_) {
      if (statistic.players_count > i
          && ((WORLD_WEIGHT * (statistic.worlds_count + 1) + CLIENT_WEIGHT * (statistic.players_count + 1))
              < SERVER_WEIGHT * MAX_CPU_LOAD)) {
        i   = statistic.players_count;
        con = conn;
      }
    }
    // if (con == -1) {
    //   return currentPort_++;
    // }

    auto& stat = statistic_[con];
    ++stat.f_worlds_count;
    ++stat.f_players_count;
    return stat.port;
  }

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

  StateMachine stateMachine_;

  int currentPort_ = 6655;
};