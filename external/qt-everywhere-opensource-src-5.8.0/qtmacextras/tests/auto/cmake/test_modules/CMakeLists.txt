
cmake_minimum_required(VERSION 2.8)

project(test_modules)

find_package(Qt5MacExtras 5.0.0 REQUIRED)
include_directories(${Qt5MacExtras_INCLUDE_DIRS})
add_definitions(${Qt5MacExtras_DEFINITIONS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Core_EXECUTABLE_COMPILE_FLAGS}")

add_executable(module_includes_exe "${CMAKE_CURRENT_SOURCE_DIR}/main.mm")
target_link_libraries(module_includes_exe  Qt5::MacExtras)
