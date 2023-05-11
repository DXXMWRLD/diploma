#include "client.h"
#include "sample/json_reader.h"
#include "sample/cpu_usage.h"
#include "sample/colors.h"

Client::Client(const std::string& address, uint16_t port)
    : connection_(address, port) {
  std::cout << CYN "Starting client connection to  " << address << ":" << port << std::endl;
  connection_.run();
}


void Client::pollConnectionStateChanges() {
  connection_.pollConnectionStateChanges();
}


void Client::netThreadRunFunc() {
  for (int i(0); isRunning_; ++i) {
    processIncomingMessages();
    pollConnectionStateChanges();
    if (i % 25 == 0 && i > 50) {
      json message       = {{"health_check", i}};
      string message_str = message.dump();


      int bytes_sent = SteamNetworkingSockets()->SendMessageToConnection(connection_.steamConnection_,
                                                                         message_str.c_str(), message_str.size(),
                                                                         k_nSteamNetworkingSend_Reliable, nullptr);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}


bool Client::receiveMessage() {
  ISteamNetworkingMessage* incoming_message = nullptr;
  const int num_msg
      = SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(connection_.pollGroup_, &incoming_message, 1);

  if (num_msg == 0) {
    return false;
  }

  auto j
      = OnMessageReceived((const char*)incoming_message->m_pData, incoming_message->m_cbSize, incoming_message->m_conn);

  if (j.contains("address") && j.contains("port")) {
    auto address = j["address"].get<std::string>();
    auto port    = j["port"].get<uint16_t>();
    std::cout << RED << "Trying to connect to " << address << ":" << port << std::endl;
    connection_.close();
    connection_.init(address, port);
    connection_.run();
  } else {
    //
  }
  return true;
}


void Client::processIncomingMessages() {
  while (receiveMessage()) {
  }
}
