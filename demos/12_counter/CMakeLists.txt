add_executable(counter_intro counter_intro.cc)
target_link_libraries(counter_intro workflow spdlog::spdlog)

add_executable(counter_to_parallel counter_to_parallel.cc)
target_link_libraries(counter_to_parallel workflow spdlog::spdlog)

