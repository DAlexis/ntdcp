cmake_minimum_required(VERSION 3.2)

find_package(GTest)

if(GTest_FOUND)
project(ntdcp-tests)


add_executable(${PROJECT_NAME}
    test-package.cpp
    test-channel.cpp
    test-caching-set.cpp
    test-network-simple.cpp
    test-transport.cpp
    test-helpers.hpp
    test-helpers.cpp)

target_link_libraries (${PROJECT_NAME}
    ntdcp
    GTest::Main
)

add_test(NAME ${PROJECT_NAME}
         COMMAND ${PROJECT_NAME})
endif()
