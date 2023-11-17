project(ntdcp)

add_library(${PROJECT_NAME}
    src/ntdcp.cpp

    ntdcp/ntdcp.hpp
    ntdcp/package.hpp
    src/package.cpp
    ntdcp/system-driver.hpp
    ntdcp/utils.hpp
    src/utils.cpp
    ntdcp/node.hpp)

target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR})
