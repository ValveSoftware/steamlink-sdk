
cmake_minimum_required(VERSION 2.8)

project(test_qremoteobjects_module)

find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Network REQUIRED)

find_package(Qt5RemoteObjects REQUIRED)

include_directories(
    ${Qt5RemoteObjects_INCLUDE_DIRS}
    ${Qt5Core_INCLUDE_DIRS}
    ${Qt5Network_INCLUDE_DIRS}
)

add_definitions(
    ${Qt5RemoteObjects_DEFINITIONS}
    ${QtCore_DEFINITIONS}
    ${Qt5Network_DEFINITIONS}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Core_EXECUTABLE_COMPILE_FLAGS} ${Qt5Network_EXECUTABLE_COMPILE_FLAGS} ${Qt5Replicant_EXECUTABLE_COMPILE_FLAGS}")

add_executable(mainapp main.cpp)

target_link_libraries(mainapp
    ${Qt5RemoteObjects_LIBRARIES}
    ${Qt5Core_LIBRARIES}
    ${Qt5Network_LIBRARIES}
)
