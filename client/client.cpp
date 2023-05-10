#include "client.h"

Client::Client(const std::string& address, uint16_t port)
    : connection_(address, port) {
  connection_.run();
}
