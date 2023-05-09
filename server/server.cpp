#include "server.h"
#include <thread>
#include <iostream>
#include <nlohmann/json.hpp>


using namespace std;
using json = nlohmann::json;

Server* Server::callbackInstance_ = nullptr;

Server::Server(uint16_t port)
    : port_(port)
    , pollGroup_(SteamNetworkingSockets()->CreatePollGroup()) {
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

        // Spew something to our own log.  Note that because we put their nick
        // as the connection description, it will show up, along with their
        // transport-specific data (e.g. their IP address)
        // gameWorldManager_.scheduleRemoveClientFromWorld(info->m_hConn,                //
        //                                                 itClient->second.worldId_,    //
        //                                                 itClient->second.playerUuid_, //
        //                                                 std::nullopt);
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
      break;
    } else {
      std::cout << "already connected" << std::endl;
    }
  }

  case k_ESteamNetworkingConnectionState_Connected:
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


void Server::processIncomingMessages() {
}

void Server::netThreadRunFunc() {
  while (serverIsRunning_) {
    processIncomingMessages();
    pollConnectionStateChanges();
    // messageManager_->sendMessages(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
}
