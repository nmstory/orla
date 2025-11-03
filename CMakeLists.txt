cmake_minimum_required(VERSION 3.2)
project(Orla LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external)

file(GLOB_RECURSE ORLA_SOURCES src/*.cpp)
file(GLOB_RECURSE ORLA_HEADERS include/*.h)

add_executable(orla ${ORLA_SOURCES} ${ORLA_HEADERS})
target_include_directories(orla PRIVATE include)
target_link_libraries(orla PRIVATE juntos_lib)
