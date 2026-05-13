add_subdirectory(juntos)

option(ENABLE_PUSH        "Enable Prometheus push gateway" OFF)
option(ENABLE_COMPRESSION "Enable zlib compression"        OFF)
option(ENABLE_TESTING     "Enable tests"                   OFF)
add_subdirectory(prometheus-cpp)

add_library(orla_external INTERFACE)
target_link_libraries(orla_external INTERFACE juntos prometheus-cpp::pull prometheus-cpp::core)
