cmake_minimum_required(VERSION 3.10)
project(SDL3Project)

set(CMAKE_CXX_STANDARD 17)

# Path to SDL3
set(SDL3_DIR ${CMAKE_SOURCE_DIR}/libs/SDL3)

include_directories(${SDL3_DIR}/include)
link_directories(${SDL3_DIR}/lib/x64)

add_executable(${PROJECT_NAME} main.cpp)

# Link SDL3
target_link_libraries(${PROJECT_NAME} SDL3)