add_executable(orla_tests
    test_main.cpp
)

target_link_libraries(orla_tests PRIVATE GTest::gtest_main)
target_include_directories(orla_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

include(GoogleTest)
gtest_discover_tests(orla_tests)
