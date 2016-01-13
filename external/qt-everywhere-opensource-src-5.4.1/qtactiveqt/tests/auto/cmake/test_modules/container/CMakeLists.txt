
cmake_minimum_required(VERSION 2.8)

project(cmake_axcontainer)

find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Widgets REQUIRED)

find_package(Qt5AxContainer REQUIRED)

include_directories(
    ${Qt5AxContainer_INCLUDE_DIRS}
)

add_definitions(${Qt5AxContainer_DEFINITIONS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Core_EXECUTABLE_COMPILE_FLAGS}")

add_executable(axcontainerapp axcontainer.cpp)

target_link_libraries(axcontainerapp
    ${Qt5AxContainer_LIBRARIES}
)
