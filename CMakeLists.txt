cmake_minimum_required(VERSION 3.2)

project(ntdcp-lib)

add_compile_options(-Werror=return-type)

add_subdirectory(ntdcp)
add_subdirectory(tests)
