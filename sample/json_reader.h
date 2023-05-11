#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;
using namespace std;

json OnMessageReceived(const char* data, int data_len, HSteamNetConnection conn) {
  // parse json message
  try {
    json message = json::parse(data, data + data_len);
    if (!message.contains("health_check"))
      cout << "Received message: " << message.dump() << endl;
    return message;
  } catch (const exception& ex) {
    cerr << "Error: failed to parse message as json: " << ex.what() << endl;
  }
  return json();
}
