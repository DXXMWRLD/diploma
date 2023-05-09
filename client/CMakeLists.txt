add_executable(Client main.cpp
                      client.cpp
                      connection.cpp)

target_link_libraries(Client 
                      PUBLIC 
                      GameNetworkingSockets::shared)