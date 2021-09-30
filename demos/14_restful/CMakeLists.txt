add_executable(restful_server restful_server.cc base64.cc)
target_link_libraries(restful_server workflow spdlog::spdlog)

add_executable(restful_client restful_client.cc)
target_link_libraries(restful_client workflow spdlog::spdlog)
