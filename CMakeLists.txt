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
    src/display/DisplaySession.cpp
    src/display/GameplayScreen.cpp
    src/display/MenuNavigation.cpp
    src/display/MenuScreen.cpp
    src/display/NumpadScreen.cpp
    src/display/PageRenderer.cpp
    src/display/UiConfig.cpp
    src/display/UiTheme.cpp
    src/display/pages/HomePage.cpp
    src/display/pages/AboutPage.cpp
    src/display/pages/InputPresetManagerPage.cpp
    src/display/pages/InputPresetEditorPage.cpp
    src/display/pages/InputPresetDeleteConfirmPage.cpp
    src/display/pages/LeftStickCalibrationPage.cpp
    src/display/pages/LeftStickModPage.cpp
    src/display/pages/RightStickModPage.cpp
    src/display/pages/NumpadPage.cpp
    src/display/pages/SettingsPage.cpp
    src/input/EvdevInput.cpp
    src/input/InputPreset.cpp
    src/input/LeftStickCalibration.cpp
    src/input/MphAutoStickMod.cpp
    src/input/MphManualStickMod.cpp
    src/input/CursorStickMod.cpp
    src/input/Sm64DpadMod.cpp
    src/input/Sm64ManualStickMod.cpp
    src/input/Sm64StickMod.cpp
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
    unset(SDL2_IMAGE_LIBRARY CACHE)
    find_library(SDL2_IMAGE_LIBRARY NAMES SDL2_image REQUIRED)
    target_link_libraries(widemelon-client PRIVATE ${SDL2_IMAGE_LIBRARY})
    target_compile_definitions(widemelon-client PRIVATE WIDEMELON_HAVE_SDL_IMAGE=1)

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
