#include <iostream>
#include <nlohmann/json.hpp>
#include <steam/steamnetworkingsockets.h>

using namespace std;
using json = nlohmann::json;

int main() {
  // // initialize steam networking
  // if (!GameNetworkingSockets_Init(nullptr)) {
  //   cerr << "Error: failed to initialize steam networking." << endl;
  //   return 1;
  // }

  // // create a client socket
  // HSteamNetConnection conn = SteamNetworkingSockets_CreateConnectionSocket("127.0.0.1:1234", 0, nullptr);
  // if (conn == k_HSteamNetConnection_Invalid) {
  //   cerr << "Error: failed to create connection socket." << endl;
  //   return 1;
  // }

  // // send a json message
  // json message       = {{"name", "Alice"}, {"age", 30}};
  // string message_str = message.dump();
  // int bytes_sent     = SteamNetworkingSockets_SendMessageToConnection(conn, message_str.c_str(), message_str.size(),
  //                                                                     k_nSteamNetworkingSend_Reliable, nullptr);
  // if (bytes_sent < 0) {
  //   cerr << "Error: failed to send message." << endl;
  //   return 1;
  // }

  // // close the connection
  // SteamNetworkingSockets_CloseConnection(conn, 0, nullptr, false);

  // // shutdown steam networking
  // GameNetworkingSockets_Kill();

  // return 0;
}