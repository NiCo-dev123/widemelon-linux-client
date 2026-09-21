cmake_minimum_required(VERSION 3.16)

project(widemelon-linux-client VERSION 0.1.0 LANGUAGES CXX)

add_executable(widemelon-client
    src/main.cpp
    src/config/Config.cpp
    src/input/EvdevInput.cpp
    src/network/WebSocketClient.cpp
    src/protocol/WideMelonProtocol.cpp
)

target_include_directories(widemelon-client PRIVATE src)
target_compile_features(widemelon-client PRIVATE cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(widemelon-client PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_subdirectory(tests)
