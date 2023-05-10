add_executable(Balancer 
          main.cpp
          balancer.cpp
          ../server/server.cpp
)

target_link_libraries(Balancer 
                      PUBLIC 
                      GameNetworkingSockets::shared)