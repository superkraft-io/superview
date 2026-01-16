cmake_minimum_required(VERSION 3.16)
project(msdf-gpu VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch SDL2 for OpenGL context creation
include(FetchContent)

FetchContent_Declare(
  SDL2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-2.28.5
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL2)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Main executable
add_executable(msdf-gpu
  main.cpp
)

target_include_directories(msdf-gpu PRIVATE
  ${SDL2_SOURCE_DIR}/include
)

target_link_libraries(msdf-gpu PRIVATE
  SDL2::SDL2
  SDL2::SDL2main
  OpenGL::GL
)

# Copy SDL2.dll to output directory on Windows
if(WIN32)
  add_custom_command(TARGET msdf-gpu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:SDL2::SDL2>
    $<TARGET_FILE_DIR:msdf-gpu>
  )
endif()
