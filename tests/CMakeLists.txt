add_executable(widemelon-client-tests
    protocol_test.cpp
    ../src/config/Config.cpp
    ../src/protocol/WideMelonProtocol.cpp
)

target_compile_features(widemelon-client-tests PRIVATE cxx_std_17)
target_include_directories(widemelon-client-tests PRIVATE ../src)
if(JPEG_FOUND)
    target_sources(widemelon-client-tests PRIVATE ../src/video/JpegDecoder.cpp)
    target_link_libraries(widemelon-client-tests PRIVATE JPEG::JPEG)
    target_compile_definitions(widemelon-client-tests PRIVATE WIDEMELON_HAVE_JPEG=1)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(widemelon-client-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME widemelon-client-tests COMMAND widemelon-client-tests)
