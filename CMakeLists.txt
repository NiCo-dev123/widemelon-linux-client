cmake_minimum_required(VERSION 3.16)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" WIDEMELON_VERSION)
string(STRIP "${WIDEMELON_VERSION}" WIDEMELON_VERSION)

project(widemelon-linux-client VERSION ${WIDEMELON_VERSION} LANGUAGES CXX)

find_package(SDL2 REQUIRED)
find_package(Threads REQUIRED)
if(CMAKE_CROSSCOMPILING)
    find_package(JPEG REQUIRED)
else()
    find_package(JPEG QUIET)
endif()

add_executable(widemelon-client
    src/main.cpp
    src/common/Logger.cpp
    src/config/Config.cpp
    src/display/ConfigScreen.cpp
    src/input/EvdevInput.cpp
    src/network/WebSocketClient.cpp
    src/protocol/WideMelonProtocol.cpp
)

target_include_directories(widemelon-client PRIVATE src)
target_compile_features(widemelon-client PRIVATE cxx_std_17)
target_compile_definitions(widemelon-client PRIVATE WIDEMELON_VERSION=\"${WIDEMELON_VERSION}\")
target_link_libraries(widemelon-client PRIVATE SDL2::SDL2 Threads::Threads)

if(JPEG_FOUND)
    target_sources(widemelon-client PRIVATE src/video/JpegDecoder.cpp)
    target_link_libraries(widemelon-client PRIVATE JPEG::JPEG)
    target_compile_definitions(widemelon-client PRIVATE WIDEMELON_HAVE_JPEG=1)
endif()

if(CMAKE_CROSSCOMPILING)
    unset(SDL2_TTF_LIBRARY CACHE)
    find_library(SDL2_TTF_LIBRARY NAMES SDL2_ttf REQUIRED)
    target_link_libraries(widemelon-client PRIVATE ${SDL2_TTF_LIBRARY})
    target_compile_definitions(widemelon-client PRIVATE WIDEMELON_HAVE_SDL_TTF=1)
    target_link_libraries(widemelon-client PRIVATE pthread)
endif()

if(CMAKE_CROSSCOMPILING AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(widemelon-client PRIVATE -static-libstdc++ -static-libgcc)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(widemelon-client PRIVATE -Wall -Wextra -Wpedantic)
endif()
