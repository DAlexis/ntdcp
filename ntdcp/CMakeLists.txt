project(ntdcp)

add_library(${PROJECT_NAME}
    src/ntdcp.cpp

    ntdcp/ntdcp.hpp
    ntdcp/package.hpp
    src/package.cpp
    ntdcp/system-driver.hpp
    ntdcp/utils.hpp
    src/utils.cpp
    ntdcp/node.hpp
    ntdcp/channel.hpp
    src/channel.cpp
    src/node.cpp
    ntdcp/network.hpp
    src/network.cpp
    ntdcp/caching-set.hpp
    ntdcp/virtual-device.hpp
    src/virtual-device.cpp
    ntdcp/transport.hpp
    src/transport.cpp
    ntdcp/socket-datagram.hpp
    src/socket-datagram.cpp
    ntdcp/synchronization.hpp
    ntdcp/socket-stable.hpp
    src/socket-stable.cpp)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR})
