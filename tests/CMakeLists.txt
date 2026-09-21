add_executable(widemelon-client-tests
    protocol_test.cpp
    ../src/config/Config.cpp
)

target_compile_features(widemelon-client-tests PRIVATE cxx_std_17)
target_include_directories(widemelon-client-tests PRIVATE ../src)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(widemelon-client-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME widemelon-client-tests COMMAND widemelon-client-tests)
