cmake_minimum_required(VERSION 3.24)

project(ws-streaming-examples LANGUAGES CXX)

find_package(ws-streaming REQUIRED)

add_subdirectory(../../examples examples)
