#include "server.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include "sample/cpu_usage.h"
#include "sample/json_reader.h"
#include "sample/colors.h"
#include "sample/settings.h"


using namespace std;
using json = nlohmann::json;

Server* Server::callbackInstance_ = nullptr;

Server::Server(uint16_t port)
    : port_(port)
    , pollGroup_(SteamNetworkingSockets()->CreatePollGroup())
    , balancerPollGroup_(SteamNetworkingSockets()->CreatePollGroup()) {

  std::cout << "SERVER " << RED "Starting server on port: " << port_ << std::endl;
  serverAddr_.Clear();
  serverAddr_.m_port = port_;

  SteamNetworkingConfigValue_t opt{};
  opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)netConnectionStatusChangeCallBack);

  listenSocket_ = SteamNetworkingSockets()->CreateListenSocketIP(serverAddr_, 1, &opt);

  if (listenSocket_ == k_HSteamListenSocket_Invalid) {
    std::ostringstream error_msg;
    error_msg << "Failed to listen on port: " << port_;
    throw std::runtime_error(error_msg.str());
  }
}


void Server::connectBalancer() {
  SteamNetworkingIPAddr new_addr{};
  new_addr.Clear();
  string address = "127.0.0.1";
  new_addr.ParseString(address.c_str());
  new_addr.m_port = 8080;
  SteamNetworkingConfigValue_t opt{};
  opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)netConnectionStatusChangeCallBack);

  auto newConnection = SteamNetworkingSockets()->ConnectByIPAddress(new_addr, 1, &opt);

  if (newConnection == k_HSteamNetConnection_Invalid) {
    throw std::runtime_error("Failed to create connection");
  }

  SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_NagleTime, 0);
  int32_t p;
  size_t cbP = sizeof(p);
  if (auto getOk = SteamNetworkingUtils()->GetConfigValue(k_ESteamNetworkingConfig_NagleTime,
                                                          k_ESteamNetworkingConfig_Global, 0, nullptr, &p, &cbP);
      getOk == k_ESteamNetworkingGetConfigValue_OK || //
      getOk == k_ESteamNetworkingGetConfigValue_OKInherited) {
  } else {
    throw std::runtime_error("Failed to get global Nagle time");
  }
}


void Server::run() {
  serverIsRunning_ = true;
  netThread_       = std::thread(&Server::netThreadRunFunc, this);
}


void Server::addConnectionToPollGroup(HSteamNetConnection conn) const {
  SteamNetworkingSockets()->SetConnectionPollGroup(conn, pollGroup_);
}


void Server::netConnectionStatusChangeCallBack(SteamNetConnectionStatusChangedCallback_t* info) {
  callbackInstance_->onSteamNetConnectionStatusChanged(info);
}


void Server::pollConnectionStateChanges() {
  callbackInstance_ = this;
  SteamNetworkingSockets()->RunCallbacks();
}


void Server::onSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) {
  // What's the state of the connection?
  switch (info->m_info.m_eState) {
  case k_ESteamNetworkingConnectionState_None:
    // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
    break;

  case k_ESteamNetworkingConnectionState_ClosedByPeer:
  case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
    std::cout << "SERVER " << RED "Disconnecting " << info->m_hConn << ":" << info->m_info.m_addrRemote.m_port
              << std::endl;

    // Ignore if they were not previously connected.  (If they disconnected
    // before we accepted the connection.)
    if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connected) {
      // Locate the client.  Note that it should have been found, because this
      // is the only codepath where we remove clients (except on shutdown),
      // and connection change callbacks are dispatched in queue order.
      if (auto itClient = clients_.find(info->m_hConn); itClient != clients_.end()) {
        // Select appropriate log messages
        // Note that here we could check the reason code to see if
        // it was a "usual" connection or an "unusual" one.
        const char* pszDebugLogAction
            = (info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)
                  ? "problem detected locally"
                  : "closed by peer";
        cerr << pszDebugLogAction << endl;
        // TODO: Remove from world
        clients_.erase(itClient);
      }
    }

    // Clean up the connection.  This is important!
    // The connection is "closed" in the network sense, but
    // it has not been destroyed.  We must close it on our end, too
    // to finish up.  The reason information do not matter in this case,
    // and we cannot linger because it's already closed on the other end,
    // so we just pass 0's.
    SteamNetworkingSockets()->CloseConnection(info->m_hConn, 0, nullptr, false);
    break;
  }

  case k_ESteamNetworkingConnectionState_Connecting: {
    std::cout << "SERVER " << RED "Connecting " << info->m_hConn << ":" << info->m_info.m_addrRemote.m_port
              << std::endl;

    if (!isBalancerConnected_) {
      if (SteamNetworkingSockets()->AcceptConnection(info->m_hConn) != k_EResultOK) {
        // This could fail.  If the remote host tried to connect, but then
        // disconnected, the connection may already be half closed.  Just
        // destroy whatever we have on our side.
        SteamNetworkingSockets()->CloseConnection(info->m_hConn, 0, nullptr, false);
        break;
      }

      balancerConnection_ = info->m_hConn;
      std::cout << "SERVER " << RED "Balancer connected " << balancerConnection_ << std::endl;

      isBalancerConnected_ = true;
      SteamNetworkingSockets()->SetConnectionPollGroup(balancerConnection_, balancerPollGroup_);

    } else {
      // This must be a new connection
      if (clients_.count(info->m_hConn) == 0) {

        // A client is attempting to connect
        // Try to accept the connection.
        if (SteamNetworkingSockets()->AcceptConnection(info->m_hConn) != k_EResultOK) {
          // This could fail.  If the remote host tried to connect, but then
          // disconnected, the connection may already be half closed.  Just
          // destroy whatever we have on our side.
          SteamNetworkingSockets()->CloseConnection(info->m_hConn, 0, nullptr, false);
          break;
        }

        // Assign the poll group
        addConnectionToPollGroup(info->m_hConn);

        auto world_id = worldDistribution();
        if (worlds_.count(world_id) == 0) {
          runWorld(world_id);
        }

        clients_.emplace(info->m_hConn, Server::ClientInfo(world_id));
        break;
      }
    }
  }

  case k_ESteamNetworkingConnectionState_Connected:
    cerr << "SERVER " << CYN "Connected " << info->m_hConn << ":" << info->m_info.m_addrRemote.m_port << std::endl;
    // We will get a callback immediately after accepting the connection.
    // Since we are the server, we can ignore this, it's not news to us.
    break;

  default:
    // Silences -Wswitch
    break;
  }
}


bool Server::receiveMessage() {
  ISteamNetworkingMessage* incoming_message = nullptr;
  const int num_msg = SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(pollGroup_, &incoming_message, 1);

  if (num_msg == 0) {
    return false;
  }

  OnMessageReceived((const char*)incoming_message->m_pData, incoming_message->m_cbSize, incoming_message->m_conn);

  return true;
}


void Server::processIncomingMessages() {
  while (receiveMessage()) {
  }
}


int32_t Server::worldDistribution() {
  return 1 + rand() % WORLDS_MAX_COUNT;
}


void Server::addClientToWorld(int32_t world_id) {
}


void Server::runWorld(int32_t world_id) {
  worlds_.emplace(world_id);
}


void Server::netThreadRunFunc() {
  float cpu_sum                     = 0;
  constexpr int cpu_check_frequency = CPU_CHECK_INTERVAL / SYNC_INTERVAL;
  size_t previous_idle_time = 0, previous_total_time = 0;

  init();

  int64_t empty_server_time = 0;
  for (int i(0); serverIsRunning_; ++i) {
    auto start = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::high_resolution_clock::now().time_since_epoch())
                     .count();

    // std::cout << "SERVER " <<  "start: " << start << std::endl;

    processIncomingMessages();
    pollConnectionStateChanges();

    if (clients_.empty()) {
      if (empty_server_time == 0) {
        empty_server_time = start;
      } else if (start - empty_server_time > EMPTY_SERVER_CLOSE_DELAY) {
        std::cout << "SERVER "
                  << "Empty server closed: Empty since " << empty_server_time << " Time now " << start << std::endl;
        serverIsRunning_ = false;
      }
      continue;
    }

    empty_server_time = 0;

    std::map<int, int> worldsActivities;
    for (auto& [conn, clientInfo] : clients_) {
      worldsActivities[clientInfo.worldId_]++;
    }

    for (auto it = worlds_.begin(); it != worlds_.end();) {
      if (worldsActivities.count(*it) == 0) {
        it = worlds_.erase(it);
      } else
        ++it;
    }

    cpu_sum += CPUCheck(previous_idle_time, previous_total_time);

    if (i % cpu_check_frequency == 0) {
      std::cout << "SERVER " << RED "Average CPU usage for : " << cpu_sum / (float)cpu_check_frequency << '%'
                << std::endl;
      std::cout << "SERVER " << RED "Average CPU usage for current process: " << getCurrentValue() << '%' << std::endl;
      cpu_sum = 0;
    }

    auto tick_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::high_resolution_clock::now().time_since_epoch())
                         .count()
                     - start;
    // std::cout << "SERVER " <<  "tick_time: " << tick_time << std::endl;

    if (tick_time < SYNC_INTERVAL)
      std::this_thread::sleep_for(std::chrono::milliseconds(SYNC_INTERVAL - tick_time));
  }

  json j{{"message", "Server stopped"}};
  string message_str = j.dump();
  int bytes_sent     = SteamNetworkingSockets()->SendMessageToConnection(
          balancerConnection_, message_str.c_str(), message_str.size(), k_nSteamNetworkingSend_Reliable, nullptr);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  SteamNetworkingSockets()->CloseListenSocket(listenSocket_);

  std::cout << "SERVER " << RED "Server stopped" << std::endl;
}
