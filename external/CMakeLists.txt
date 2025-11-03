add_subdirectory(juntos)

add_library(orla_external INTERFACE)
target_link_libraries(orla_external INTERFACE juntos)
