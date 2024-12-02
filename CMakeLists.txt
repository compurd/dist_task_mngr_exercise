cmake_minimum_required(VERSION 3.15)
project(Beemo)

set(CURL_LIBRARY "-lcurl")
find_package(CURL REQUIRED)
add_executable(Tester Tester.cpp)
include_directories(${CURL_INCLUDE_DIR})
target_link_libraries(Tester CURL::libcurl)

add_executable(DistributedTaskManager
    DistributedTaskManager.cpp
    MockTask.cpp 
    Utils.cpp 
)

target_include_directories(DistributedTaskManager PRIVATE include)

find_package(Crow REQUIRED)
target_link_libraries(DistributedTaskManager Crow::Crow)

find_package(Boost REQUIRED)
target_link_libraries(DistributedTaskManager ${Boost_Libraries})