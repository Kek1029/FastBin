cmake_minimum_required(VERSION 4.2)
project(FastBin LANGUAGES CXX)

add_library(FastBin INTERFACE)

target_include_directories(FastBin INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(FastBin INTERFACE cxx_std_20)

add_subdirectory(examples)