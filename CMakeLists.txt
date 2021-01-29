cmake_minimum_required(VERSION 2.8)

set(CMAKE_BUILD_TYPE "Debug")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Wall -g2 -ggdb -ldl -pthread")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -Wall -g2 -ggdb -ldl -pthread")

project(ConnectionPool)

add_executable(${PROJECT_NAME} "main.cpp" "Connection.h" "ObjectPool.h")
