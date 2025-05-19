cmake_minimum_required(VERSION 3.10.0)
project(KSolveAStar VERSION 0.1.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 20)

if(MSVC)
add_compile_options(/sdl-)
endif()

add_library(KSolveAStar Game.cpp GameStateMemory.cpp KSolveAStar.cpp MoveStorage.cpp)

target_include_directories(KSolveAStar PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})