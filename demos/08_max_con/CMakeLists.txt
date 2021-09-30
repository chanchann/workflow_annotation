add_executable(max_con_server server.cc)
target_link_libraries(max_con_server workflow spdlog::spdlog)

add_executable(max_con_client1 client01.cc)
target_link_libraries(max_con_client1 workflow spdlog::spdlog)

add_executable(max_con_client2 client02.cc)
target_link_libraries(max_con_client2 workflow spdlog::spdlog)
