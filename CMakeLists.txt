cmake_minimum_required(VERSION 3.10)

project(MFSC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(base_lib STATIC
    src/data.cc
)

target_include_directories(base_lib PUBLIC
    include
)

add_executable(peer
    src/peer.cc
)


target_link_libraries(peer PRIVATE base_lib)
