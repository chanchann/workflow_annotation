cmake_minimum_required(VERSION 2.8)

project(workflowdemo C CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

set(CXX_FLAGS
 -g
 -std=c++11 
 -rdynamic
 -pthread   # check
 )
if(CMAKE_BUILD_BITS EQUAL 32)
  list(APPEND CXX_FLAGS "-m32")
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  list(APPEND CXX_FLAGS "-Wno-null-dereference")
  list(APPEND CXX_FLAGS "-Wno-sign-conversion")
  list(APPEND CXX_FLAGS "-Wno-unused-local-typedef")
  list(APPEND CXX_FLAGS "-Wthread-safety")
  list(REMOVE_ITEM CXX_FLAGS "-rdynamic")
endif()
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

include_directories(${CMAKE_SOURCE_DIR}/3rd)

set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# add_subdirectory(${PROJECT_SOURCE_DIR}/01_echo)
# add_subdirectory(${PROJECT_SOURCE_DIR}/02_stop)
# add_subdirectory(${PROJECT_SOURCE_DIR}/03_parallel)
# add_subdirectory(${PROJECT_SOURCE_DIR}/04_wget)
# add_subdirectory(${PROJECT_SOURCE_DIR}/05_http_echo)
# add_subdirectory(${PROJECT_SOURCE_DIR}/06_http_proxy)
add_subdirectory(${PROJECT_SOURCE_DIR}/07_http)
# add_subdirectory(${PROJECT_SOURCE_DIR}/08_max_con)
# add_subdirectory(${PROJECT_SOURCE_DIR}/09_timer)
# add_subdirectory(${PROJECT_SOURCE_DIR}/10_http_forward)
# add_subdirectory(${PROJECT_SOURCE_DIR}/11_life_cycle)
# add_subdirectory(${PROJECT_SOURCE_DIR}/12_counter)

# add_subdirectory(${PROJECT_SOURCE_DIR}/14_restful)
# add_subdirectory(${PROJECT_SOURCE_DIR}/15_file)
# add_subdirectory(${PROJECT_SOURCE_DIR}/16_graph)
# add_subdirectory(${PROJECT_SOURCE_DIR}/17_go_task)

# add_subdirectory(${PROJECT_SOURCE_DIR}/19_parallel)
# add_subdirectory(${PROJECT_SOURCE_DIR}/20_dynamic)
# add_subdirectory(${PROJECT_SOURCE_DIR}/21_cmp)
# add_subdirectory(${PROJECT_SOURCE_DIR}/22_dns)
# add_subdirectory(${PROJECT_SOURCE_DIR}/23_list)
# add_subdirectory(${PROJECT_SOURCE_DIR}/24_thrd_task)
# add_subdirectory(${PROJECT_SOURCE_DIR}/25_msgque)
# add_subdirectory(${PROJECT_SOURCE_DIR}/26_resource_pool)
# add_subdirectory(${PROJECT_SOURCE_DIR}/27_parse_uri)
# add_subdirectory(${PROJECT_SOURCE_DIR}/28_string_util)
# add_subdirectory(${PROJECT_SOURCE_DIR}/29_upstream)
# add_subdirectory(${PROJECT_SOURCE_DIR}/30_kernel_comm)
# add_subdirectory(${PROJECT_SOURCE_DIR}/31_mysql)
# add_subdirectory(${PROJECT_SOURCE_DIR}/33_header)
# add_subdirectory(${PROJECT_SOURCE_DIR}/34_https)
# add_subdirectory(${PROJECT_SOURCE_DIR}/35_dismiss)
# add_subdirectory(${PROJECT_SOURCE_DIR}/36_task)

add_subdirectory(${PROJECT_SOURCE_DIR}/other)



