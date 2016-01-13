
cmake_minimum_required(VERSION 2.8)

project(test_qx11_module)

find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)

find_package(Qt5X11Extras REQUIRED)

include_directories(
    ${Qt5X11Extras_INCLUDE_DIRS}
)

add_definitions(
    ${Qt5X11Extras_DEFINITIONS}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Core_EXECUTABLE_COMPILE_FLAGS}")

add_executable(mainapp main.cpp)

target_link_libraries(mainapp
    ${Qt5X11Extras_LIBRARIES}
)
