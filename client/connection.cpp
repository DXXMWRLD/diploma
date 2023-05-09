#include "connection.h"
#include <iostream>

ClientConnectionHandler* ClientConnectionHandler::sPCallbackInstance_ = nullptr;

ClientConnectionHandler::ClientConnectionHandler(const std::string& address, uint16_t port)
    : addrServer_()
    , pollGroup_(SteamNetworkingSockets()->CreatePollGroup()) {
  addrServer_.Clear();
  addrServer_.ParseString(address.c_str());
  addrServer_.m_port = port;
}

ClientConnectionHandler::~ClientConnectionHandler() {
  close();
}

void ClientConnectionHandler::init(const std::string& address, uint16_t port) {
  SteamNetworkingIPAddr new_addr{};
  new_addr.Clear();
  new_addr.ParseString(address.c_str());
  new_addr.m_port = port;

  if (new_addr == addrServer_) {
    return;
  }

  setNewServerAddress(std::move(new_addr));
}

void ClientConnectionHandler::pollConnectionStateChanges() {
  sPCallbackInstance_ = this;
  SteamNetworkingSockets()->RunCallbacks();
}

void ClientConnectionHandler::setNewServerAddress(SteamNetworkingIPAddr&& new_addr) {
  addrServer_.Clear();
  memcpy(&addrServer_, &new_addr, sizeof(SteamNetworkingIPAddr));
  char addr[SteamNetworkingIPAddr::k_cchMaxString];
  addrServer_.ToString(addr, sizeof(addr), true);
}

void ClientConnectionHandler::run() {
  // Start connecting
  char addr[SteamNetworkingIPAddr::k_cchMaxString];
  addrServer_.ToString(addr, sizeof(addr), true);
  SteamNetworkingConfigValue_t opt{};
  opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)steamNetConnectionStatusChangedCallback);
  steamConnection_ = SteamNetworkingSockets()->ConnectByIPAddress(addrServer_, 1, &opt);

  if (steamConnection_ == k_HSteamNetConnection_Invalid) {
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

  SteamNetworkingSockets()->SetConnectionPollGroup(steamConnection_, pollGroup_);

  connectionIsRun_ = true;
}

void ClientConnectionHandler::close() {
  SteamNetworkingSockets()->CloseConnection(steamConnection_, k_ESteamNetConnectionEnd_App_Min, nullptr, 0);
}

void ClientConnectionHandler::onSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo) {
  // What's the state of the connection?
  switch (pInfo->m_info.m_eState) {
  case k_ESteamNetworkingConnectionState_None:
    // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
    break;

  case k_ESteamNetworkingConnectionState_ClosedByPeer:
  case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
    // Print an appropriate message
    if (pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connecting) {
      // Note: we could distinguish between a timeout, a rejected connection,
      // or some other transport problem.
      std::cout << "We sought the remote host, yet our efforts were met with defeat. " << pInfo->m_info.m_szEndDebug
                << std::endl;
    } else if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
      std::cout << "Alas, troubles beset us; we have lost contact with the host. " << pInfo->m_info.m_szEndDebug
                << std::endl;
    } else {
      // NOTE: We could check the reason code for a normal disconnection
      std::cout << pInfo->m_info.m_szEndDebug << std::endl;
    }

    // Clean up the connection.  This is important!
    // The connection is "closed" in the network sense, but
    // it has not been destroyed.  We must close it on our end, too
    // to finish up.  The reason information do not matter in this case,
    // and we cannot linger because it's already closed on the other end,
    // so we just pass 0's.

    // NOTE: We can't just call close(), cause it would try to join connectionThread_
    // and we are already in it.
    connectionIsRun_ = false;
    SteamNetworkingSockets()->CloseConnection(steamConnection_, k_ESteamNetConnectionEnd_Remote_Timeout, nullptr, 0);
    steamConnection_ = k_HSteamNetConnection_Invalid;
    break;
  }

  case k_ESteamNetworkingConnectionState_Connecting:
    // timer_guard_connect_ = new timer_guard(timerLogger_, "connect_time");
    // We will get this callback when we start connecting.
    // We can ignore this.
    break;

  case k_ESteamNetworkingConnectionState_Connected: {
    // TODO
    break;
  }

  default:
    // Silences -Wswitch
    break;
  }
}
