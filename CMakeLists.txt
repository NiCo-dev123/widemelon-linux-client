cmake_minimum_required(VERSION 3.16)

project(widemelon-linux-client VERSION 0.1.0 LANGUAGES CXX)

find_package(SDL2 REQUIRED)

add_executable(widemelon-client
    src/main.cpp
    src/config/Config.cpp
    src/display/ConfigScreen.cpp
    src/input/EvdevInput.cpp
    src/network/WebSocketClient.cpp
    src/protocol/WideMelonProtocol.cpp
)

target_include_directories(widemelon-client PRIVATE src)
target_compile_features(widemelon-client PRIVATE cxx_std_17)
target_link_libraries(widemelon-client PRIVATE SDL2::SDL2)

if(CMAKE_CROSSCOMPILING AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(widemelon-client PRIVATE -static-libstdc++ -static-libgcc)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(widemelon-client PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_subdirectory(tests)
