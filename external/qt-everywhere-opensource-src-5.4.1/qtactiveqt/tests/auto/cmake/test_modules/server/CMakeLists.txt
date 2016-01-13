
cmake_minimum_required(VERSION 2.8)

project(cmake_axserver)

find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Widgets REQUIRED)

find_package(Qt5AxServer REQUIRED)

include_directories(
    ${Qt5AxServer_INCLUDE_DIRS}
)

add_definitions(${Qt5AxServer_DEFINITIONS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Core_EXECUTABLE_COMPILE_FLAGS}")

add_executable(axserverapp WIN32 axserver.cpp)

target_link_libraries(axserverapp
    ${Qt5AxServer_LIBRARIES}
)
