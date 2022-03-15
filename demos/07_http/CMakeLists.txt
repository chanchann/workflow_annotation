add_executable(07_http_req 07_http_req.cc)
target_link_libraries(07_http_req workflow)

# add_executable(http_series http_series.cc)
# target_link_libraries(http_series workflow spdlog::spdlog)

# add_executable(http_parallel http_parallel.cc)
# target_link_libraries(http_parallel workflow spdlog::spdlog)

add_executable(http_echo_defer http_echo_defer.cc)
target_link_libraries(http_echo_defer workflow)

add_executable(07_http_no_reply 07_http_no_reply.cc)
target_link_libraries(07_http_no_reply workflow)

add_executable(07_http_simple 07_http_simple.cc)
target_link_libraries(07_http_simple workflow)