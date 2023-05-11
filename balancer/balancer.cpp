#include "balancer.h"
#include <thread>
#include <iostream>
#include <stdlib.h>
#include <set>
#include <chrono>


using namespace std;
using json = nlohmann::json;


void StateMachine::process(const std::unordered_map<HSteamNetConnection, ServerInfo>& statistic) {
  float P = 0.f;
  float N = 0.f;

  bool good_server = false;

  for (auto& [conn, server_info] : statistic) {
    P += SERVER_WEIGHT * MAX_CPU_LOAD;
    N += WORLD_WEIGHT * (server_info.worlds_count + server_info.f_worlds_count)
         + CLIENT_WEIGHT * (server_info.players_count + server_info.f_players_count);

    if (P - N >= CLIENT_WEIGHT + WORLD_WEIGHT)
      good_server = true;
  }
  float Os = N / P;

  if (Os < 0) {
    Os = 0;
  }

  cpu_load.push_back(Os);
  ++i;


  // std::cout << "P = " << P << " N = " << N << " Os = " << Os << std::endl;

  float sum = 0;
  float avg;

  if (i < LAST_TICK_NUMBER) {
    for (auto it = cpu_load.begin(); it != cpu_load.end(); ++it) {
      sum += *it;
    }
    avg = (float)sum / (i + 1);
  } else {
    for (int j(i); j > i - LAST_TICK_NUMBER; --j) {
      sum += cpu_load[j];
    }
    avg = (float)sum / LAST_TICK_NUMBER;
  }

  if (avg < 0) {
    avg = 0;
  }

  if (Os > CRITICAL_UPPER_BOUND) {
    state = State::Increase;
    std::cout << RED "CURRENT LOAD = " << Os << " AVERAGE LOAD = " << avg << " CURRENT STATE = " << (int)state
              << std::endl;
    return;
  }

  if (Os < CRITICAL_LOWER_BOUND) {
    state = State::Decrease;
    std::cout << RED "CURRENT LOAD = " << Os << " AVERAGE LOAD = " << avg << " CURRENT STATE = " << (int)state
              << std::endl;
    return;
  }

  if (!good_server) {
    state = State::Increase;
    std::cout << RED "CURRENT LOAD = " << Os << " AVERAGE LOAD = " << avg << " CURRENT STATE = " << (int)state
              << std::endl;
    return;
  }

  if (avg < LOWER_BOUND) {
    state = State::Decrease;
  } else if (avg >= UPPER_BOUND) {
    state = State::Increase;
  } else {
    state = State::Stability;
  }

  std::cout << RED "CURRENT LOAD = " << Os << " AVERAGE LOAD = " << avg << " CURRENT STATE = " << (int)state
            << std::endl;
}


State StateMachine::getState() const {
  return state;
}


Balancer* Balancer::callbackInstance_ = nullptr;

Balancer::Balancer(uint16_t port)
    : port_(port)
    , pollGroup_(SteamNetworkingSockets()->CreatePollGroup()) {
  std::cout << CYN "BALANCER " << GRN "Starting balancer on port: " << port_ << std::endl;

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


void Balancer::run() {
  serverIsRunning_ = true;
}


void Balancer::addConnectionToPollGroup(HSteamNetConnection conn) const {
  SteamNetworkingSockets()->SetConnectionPollGroup(conn, pollGroup_);
}

void Balancer::netConnectionStatusChangeCallBack(SteamNetConnectionStatusChangedCallback_t* info) {
  callbackInstance_->onSteamNetConnectionStatusChanged(info);
}


void Balancer::pollConnectionStateChanges() {
  callbackInstance_ = this;
  SteamNetworkingSockets()->RunCallbacks();
}


nlohmann::json Balancer::serverDistribution() {
  if (currentPort_ != 6655) {
    processStateMachine();
  }
  std::cout << CYN "BALANCER Choosing server for new client..." << std::endl;
  int32_t port;
  if (stateMachine_.getState() == State::Increase) {
    port = currentPort_++;
  } else if (stateMachine_.getState() == State::Decrease) {
    port = findHighServer();
  } else {
    port = findLowServer();
  }
  std::cout << "Current State = " << (int)stateMachine_.getState() << " Port: = " << port << std::endl;
  return json{{"address", "127.0.0.1"}, {"port", port}};
}


void Balancer::startNewServer(nlohmann::json j) {
  int port       = j["port"];
  string address = j["address"];

  auto server = std::make_unique<Server>(port);
  server->run();

  SteamNetworkingIPAddr new_addr{};
  new_addr.Clear();
  new_addr.ParseString(address.c_str());
  new_addr.m_port = port;
  SteamNetworkingConfigValue_t opt{};

  auto newConnection = SteamNetworkingSockets()->ConnectByIPAddress(new_addr, 0, &opt);

  if (newConnection == k_HSteamNetConnection_Invalid) {
    throw std::runtime_error("Failed to create connection");
  }

  SteamNetworkingSockets()->SetConnectionPollGroup(newConnection, pollGroup_);
  servers_.emplace(newConnection, std::move(server));
  auto [it, success]       = statistic_.emplace(newConnection, ServerInfo());
  it->second.address       = address;
  it->second.port          = port;
  it->second.worlds_count  = 0;
  it->second.players_count = 0;

  processStateMachine();
}


void Balancer::onSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) {
  // What's the state of the connection?
  switch (info->m_info.m_eState) {
  case k_ESteamNetworkingConnectionState_None:
    // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
    break;

  case k_ESteamNetworkingConnectionState_ClosedByPeer:
  case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
    // std::cout << "BALANCER " << GRN "Disconnecting " << info->m_hConn << ":" << info->m_info.m_addrRemote.m_port
    //           << std::endl;
    // Ignore if they were not previously connected.  (If they disconnected
    // before we accepted the connection.)
    if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connected) {
      const char* pszDebugLogAction
          = (info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)
                ? "problem detected locally"
                : "closed by peer";
      cerr << pszDebugLogAction << endl;
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
    // std::cout << "BALANCER " << GRN "Connecting " << info->m_hConn << ":" << info->m_info.m_addrRemote.m_port
    //           << std::endl;
    // This must be a new connection

    if (SteamNetworkingSockets()->AcceptConnection(info->m_hConn) != k_EResultOK) {
      // This could fail.  If the remote host tried to connect, but then
      // disconnected, the connection may already be half closed.  Just
      // destroy whatever we have on our side.
      SteamNetworkingSockets()->CloseConnection(info->m_hConn, 0, nullptr, false);
      break;
    }
    addConnectionToPollGroup(info->m_hConn);

    // std::cout << "BALANCER " << GRN "ADDED TO POLL GROUP " << info->m_hConn << ":" <<
    // info->m_info.m_addrRemote.m_port
    //           << std::endl;


    auto j   = serverDistribution();
    int port = j["port"];

    bool found = false;
    for (auto& [conn, serverInfo] : statistic_) {
      if (serverInfo.port == port) {
        found = true;
        break;
      }
    }
    if (!found) {
      startNewServer(j);
    }

    string message_str = j.dump();
    int bytes_sent     = SteamNetworkingSockets()->SendMessageToConnection(
            info->m_hConn, message_str.c_str(), message_str.size(), k_nSteamNetworkingSend_Reliable, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SteamNetworkingSockets()->CloseConnection(info->m_hConn, 0, nullptr, false);
    break;
  }

  case k_ESteamNetworkingConnectionState_Connected:
    cerr << "BALANCER " << GRN "Connected " << info->m_hConn << ":" << info->m_info.m_addrRemote.m_port << std::endl;
    // We will get a callback immediately after accepting the connection.
    // Since we are the server, we can ignore this, it's not news to us.
    break;

  default:
    // Silences -Wswitch
    break;
  }
}


json readJSON(const char* data, int data_len, HSteamNetConnection conn) {
  // parse json message
  try {
    json message = json::parse(data, data + data_len);
    cout << "Received message: " << message.dump() << endl;
    return message;
  } catch (const exception& ex) {
    cerr << "Error: failed to parse message as json: " << ex.what() << endl;
  }
  return json();
}

bool Balancer::receiveMessage() {
  ISteamNetworkingMessage* incoming_message = nullptr;
  const int num_msg = SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(pollGroup_, &incoming_message, 1);

  if (num_msg == 0) {
    return false;
  }

  auto j = readJSON((const char*)incoming_message->m_pData, incoming_message->m_cbSize, incoming_message->m_conn);

  if (j.contains("message")) {
    std::cout << GRN "BALANCER "
              << "remove server with connection " << incoming_message->m_conn << std::endl;
    auto find = servers_.find(incoming_message->m_conn);
    if (find != servers_.end()) {
      find->second->netThread_.join();
      servers_.erase(find);
      SteamNetworkingSockets()->CloseConnection(incoming_message->m_conn, 0, nullptr, false);
    }
    statistic_.erase(incoming_message->m_conn);
  } else {
    auto it                    = statistic_.find(incoming_message->m_conn);
    it->second.worlds_count    = j["worlds_count"];
    it->second.players_count   = j["clients_count"];
    it->second.f_players_count = 0;
    it->second.f_worlds_count  = 0;
  }

  return true;
}


void Balancer::processIncomingMessages() {
  while (receiveMessage()) {
    processStateMachine();
  }
}


void Balancer::netThreadRunFunc() {
  for (int i(0); serverIsRunning_; ++i) {
    if (i % 50 == 0) {
      using namespace std::chrono;
      time_t now = chrono::system_clock::to_time_t(chrono::system_clock::now());
      cout << put_time(localtime(&now), "%F %T") << endl;
      print();
    }
    processIncomingMessages();
    pollConnectionStateChanges();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}
