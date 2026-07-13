cmake_minimum_required(VERSION 3.24)

project(AshenDominion VERSION 0.1.0 LANGUAGES CXX)

option(ASHEN_BUILD_TESTS "Build the native simulation tests" ON)

set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
add_subdirectory(native)
