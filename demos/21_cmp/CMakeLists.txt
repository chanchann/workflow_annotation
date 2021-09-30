add_executable(21_clent01 21_clent01.cc)
target_link_libraries(21_clent01 workflow)

add_executable(test_add_header02 test02.cc)
target_link_libraries(test_add_header02 workflow)

add_executable(21_server 21_server.cc)
target_link_libraries(21_server workflow)