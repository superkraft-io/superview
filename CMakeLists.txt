cmake_minimum_required(VERSION 3.20)
project(skene VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# SDL2
FetchContent_Declare(
  SDL2
  URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.0/SDL2-2.30.0.zip
)
FetchContent_MakeAvailable(SDL2)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
)
FetchContent_GetProperties(glm)
if(NOT glm_POPULATED)
    FetchContent_Populate(glm)
    add_library(glm INTERFACE)
    target_include_directories(glm INTERFACE ${glm_SOURCE_DIR})
    add_library(glm::glm ALIAS glm)
endif()

# msdfgen (core only - no dependencies)
FetchContent_Declare(
    msdfgen
    GIT_REPOSITORY https://github.com/Chlumsky/msdfgen.git
    GIT_TAG        v1.12
)
set(MSDFGEN_CORE_ONLY ON CACHE BOOL "" FORCE)
set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDFGEN_USE_VCPKG OFF CACHE BOOL "" FORCE)
set(MSDFGEN_INSTALL OFF CACHE BOOL "" FORCE)
set(MSDFGEN_DYNAMIC_RUNTIME ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(msdfgen)

add_executable(skene src/main.cpp)

target_include_directories(skene PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(skene PRIVATE SDL2::SDL2-static SDL2::SDL2main glm::glm msdfgen::msdfgen-core)

if(WIN32)
	target_link_libraries(skene PRIVATE opengl32)
endif()
