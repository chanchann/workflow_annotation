add_executable(go go.cc)
target_link_libraries(go workflow)

add_executable(go_simple go_simple.cc)
target_link_libraries(go_simple workflow)

