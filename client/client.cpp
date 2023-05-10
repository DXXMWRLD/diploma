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
  float cpu_sum             = 0;
  int cpu_check_frequency   = 100;
  size_t previous_idle_time = 0, previous_total_time = 0;

  init();

  for (int i(0); isRunning_; ++i) {
    cpu_sum += CPUCheck(previous_idle_time, previous_total_time);

    // if (i % cpu_check_frequency == 0) {
    //   std::cout << CYN  "Average CPU usage for : " << cpu_sum / cpu_check_frequency << '%' << std::endl;
    //   std::cout << CYN  "Average CPU usage for current process: " << getCurrentValue() << '%' << std::endl;
    //   cpu_sum = 0;
    // }
    // for (float k(0); k < 110000000000000000000000000.f; ++k) {
    //   float j(1000000);
    //   while (j < 100000000000000000000000000000000000.f) {
    //     auto b = j * j - j + j / k;
    //     b *= b;
    //     (void)b;
    //     ++j;
    //   }
    //   // std::this_thread::sleep_for(std::chrono::microseconds(500));
    // }
    processIncomingMessages();
    pollConnectionStateChanges();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
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
    std::cout << RED << "Connected to " << address << ":" << port << std::endl;
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
