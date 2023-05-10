add_executable(Balancer 
          main.cpp
          balancer.cpp
)

target_link_libraries(Balancer 
                      PUBLIC 
                      GameNetworkingSockets::shared)