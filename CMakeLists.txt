cmake_minimum_required(VERSION 3.20)

project(ANNShin VERSION 1.0.0
	DESCRIPTION "A simple neuoral network"
	LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Windowed renderer (raylib). Turn OFF to build the headless core only
# (e.g. offline, or for fast batch runs): cmake -B build -DANNSHIN_WINDOW=OFF
option(ANNSHIN_WINDOW "Build the raylib windowed renderer" ON)
if(ANNSHIN_WINDOW)
  include(FetchContent)
  FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(raylib)
endif()

add_subdirectory(src)
