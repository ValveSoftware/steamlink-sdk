cmake_minimum_required(VERSION 2.8)

project(test_qremoteobjects_module)

find_package(Qt5RemoteObjects REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(MAIN_SRCS main.cpp)
qt5_generate_repc(MAIN_SRCS ../../integration/pod.rep REPLICA)
add_executable(mainapp ${MAIN_SRCS})
target_link_libraries(mainapp Qt5::RemoteObjects)
