cmake_minimum_required(VERSION 3.0)
project(test_uiplugin_module)

find_package(Qt5Widgets REQUIRED)
find_package(Qt5UiPlugin REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(my_designer_plugin my_designer_plugin.cpp)
target_link_libraries(my_designer_plugin Qt5::UiPlugin)
