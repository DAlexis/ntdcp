cmake_minimum_required(VERSION 2.8)

find_package(GTest)

if(GTest_FOUND)
project(ntdcp-tests)


add_executable(${PROJECT_NAME} placeholder.cpp)

target_link_libraries (${PROJECT_NAME}
    GTest::Main
)

add_test(NAME ${PROJECT_NAME}
         COMMAND ${PROJECT_NAME})
endif()
