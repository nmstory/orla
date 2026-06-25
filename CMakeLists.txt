cmake_minimum_required(VERSION 3.14)
project(Orla LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

option(ORLA_TSAN "Build with ThreadSanitizer" OFF)
if(ORLA_TSAN)
    message(STATUS "ThreadSanitizer enabled")
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g -O1)
    add_link_options(-fsanitize=thread)
endif()

find_package(ZLIB REQUIRED)

add_subdirectory(external)

file(GLOB_RECURSE ORLA_SOURCES src/*.cpp)
file(GLOB_RECURSE ORLA_HEADERS include/*.h)

add_executable(orla ${ORLA_SOURCES} ${ORLA_HEADERS})
target_include_directories(orla PRIVATE include ${ZLIB_INCLUDE_DIRS})
target_link_libraries(orla PRIVATE juntos_lib prometheus-cpp::pull prometheus-cpp::core ${ZLIB_LIBRARIES})

add_subdirectory(tests)
