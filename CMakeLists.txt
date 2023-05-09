set(CMAKE_CXX_STANDARD 20)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/GameNetworkingSockets/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/json/include)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/include/GameNetworkingSockets
        ${CMAKE_CURRENT_SOURCE_DIR}/include/GameNetworkingSockets/build)
add_subdirectory(server)
add_subdirectory(client)

add_executable(Balancer main.cpp)
