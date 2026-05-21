cmake_minimum_required(VERSION 3.10)
project(memory_game LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PROJECT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(PROJECT_INCLUDE_DIR ${PROJECT_SRC}/include)

add_executable(server
    src/server/server_main.cpp
    src/server/server.cpp
)
target_include_directories(server PRIVATE
    ${PROJECT_INCLUDE_DIR}
    ${PROJECT_SRC}/server
)

add_executable(client
    src/client/client_main.cpp
    src/client/client.cpp
)
target_include_directories(client PRIVATE
    ${PROJECT_INCLUDE_DIR}
    ${PROJECT_SRC}/client
)
