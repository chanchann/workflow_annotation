add_executable(http_req http_req.cc)
target_link_libraries(http_req workflow spdlog::spdlog)

add_executable(http_series http_series.cc)
target_link_libraries(http_series workflow spdlog::spdlog)

add_executable(http_parallel http_parallel.cc)
target_link_libraries(http_parallel workflow spdlog::spdlog)

add_executable(http_echo_defer http_echo_defer.cc)
target_link_libraries(http_echo_defer workflow spdlog::spdlog)

add_executable(http_no_reply http_no_reply.cc)
target_link_libraries(http_no_reply workflow spdlog::spdlog)