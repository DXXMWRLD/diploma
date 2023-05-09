add_executable(Server 
          main.cpp
          server.cpp
)

target_link_libraries(Server 
                      PUBLIC 
                      GameNetworkingSockets::shared)