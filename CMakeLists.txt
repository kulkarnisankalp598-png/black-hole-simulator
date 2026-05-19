cmake_minimum_required(VERSION 3.16)
project(BlackHoleSim)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE Release)

include(FetchContent)
FetchContent_Declare(SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        3.0.0)
FetchContent_MakeAvailable(SFML)

add_executable(BlackHoleSim src/main.cpp
        src/main.cpp)
target_link_libraries(BlackHoleSim PRIVATE SFML::Graphics SFML::Window SFML::System)

set_target_properties(BlackHoleSim PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})