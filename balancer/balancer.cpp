#include "balancer.h"
#include <thread>
#include <iostream>
// #include "sample/cpu_usage.h"
#include "sample/colors.h"
#include "sample/settings.h"
#include <stdlib.h>
#include <set>

using namespace std;
using json = nlohmann::json;


Balancer* Balancer::callbackInstance_ = nullptr;

Balancer::Balancer(uint16_t port)
    : port_(port)
    , pollGroup_(SteamNetworkingSockets()->CreatePollGroup()) {
  std::cout << "BALANCER " << GRN "Starting balancer on port: " << port_ << std::endl;

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
  return json{{"address", "127.0.0.1"}, {"port", 6655}};
}


void Balancer::startNewServer(nlohmann::json j) {
  int port = j["port"];

  auto server = std::make_unique<Server>(port);
  server->run();
  servers_.emplace(port, std::move(server));
}


void Balancer::onSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) {
  // What's the state of the connection?
  switch (info->m_info.m_eState) {
  case k_ESteamNetworkingConnectionState_None:
    std::cout << "BALANCER " << GRN "Disconnecting " << info->m_hConn << ":" << info->m_info.m_addrRemote.m_port
              << std::endl;
    // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
    break;

  case k_ESteamNetworkingConnectionState_ClosedByPeer:
  case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
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
    std::cout << "BALANCER " << GRN "Connecting " << info->m_hConn << ":" << info->m_info.m_addrRemote.m_port
              << std::endl;
    // This must be a new connection

    if (SteamNetworkingSockets()->AcceptConnection(info->m_hConn) != k_EResultOK) {
      // This could fail.  If the remote host tried to connect, but then
      // disconnected, the connection may already be half closed.  Just
      // destroy whatever we have on our side.
      SteamNetworkingSockets()->CloseConnection(info->m_hConn, 0, nullptr, false);
      break;
    }
    addConnectionToPollGroup(info->m_hConn);

    std::cout << "BALANCER " << GRN "ADDED TO POLL GROUP " << info->m_hConn << ":" << info->m_info.m_addrRemote.m_port
              << std::endl;


    auto j              = serverDistribution();
    int port            = j["port"];
    std::string address = j["address"];

    auto serverFind = servers_.find(port);
    if (serverFind == servers_.end()) {
      std::cout << "BALANCER "
                << "Starting new server" << std::endl;
      startNewServer(j);

      SteamNetworkingIPAddr new_addr{};
      new_addr.Clear();
      new_addr.ParseString(address.c_str());
      new_addr.m_port = port;
      SteamNetworkingConfigValue_t opt{};

      auto newConnection = SteamNetworkingSockets()->ConnectByIPAddress(new_addr, 0, &opt);

      if (newConnection == k_HSteamNetConnection_Invalid) {
        throw std::runtime_error("Failed to create connection");
      }

      // SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_NagleTime, 0);
      // int32_t p;
      // size_t cbP = sizeof(p);
      // if (auto getOk = SteamNetworkingUtils()->GetConfigValue(k_ESteamNetworkingConfig_NagleTime,
      //                                                         k_ESteamNetworkingConfig_Global, 0, nullptr, &p, &cbP);
      //     getOk == k_ESteamNetworkingGetConfigValue_OK || //
      //     getOk == k_ESteamNetworkingGetConfigValue_OKInherited) {
      // } else {
      //   throw std::runtime_error("Failed to get global Nagle time");
      // }

      SteamNetworkingSockets()->SetConnectionPollGroup(newConnection, pollGroup_);
    }

    string message_str = j.dump();
    int bytes_sent     = SteamNetworkingSockets()->SendMessageToConnection(
            info->m_hConn, message_str.c_str(), message_str.size(), k_nSteamNetworkingSend_Reliable, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // SteamNetworkingSockets()->CloseConnection(info->m_hConn, 0, nullptr, false);

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


void OnMessageReceived(const char* data, int data_len, HSteamNetConnection conn) {
  // parse json message
  try {
    json message = json::parse(data, data + data_len);
    cout << "Received message: " << message.dump() << endl;
  } catch (const exception& ex) {
    cerr << "Error: failed to parse message as json: " << ex.what() << endl;
  }
}


bool Balancer::receiveMessage() {
  ISteamNetworkingMessage* incoming_message = nullptr;
  const int num_msg = SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(pollGroup_, &incoming_message, 1);

  if (num_msg == 0) {
    return false;
  }

  OnMessageReceived((const char*)incoming_message->m_pData, incoming_message->m_cbSize, incoming_message->m_conn);

  return true;
}


void Balancer::processIncomingMessages() {
  while (receiveMessage()) {
  }
}


void Balancer::netThreadRunFunc() {
  for (int i(0); serverIsRunning_; ++i) {
    processIncomingMessages();
    pollConnectionStateChanges();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
}
