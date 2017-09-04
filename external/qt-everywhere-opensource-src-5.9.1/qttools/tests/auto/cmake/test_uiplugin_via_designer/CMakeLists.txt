
# Backward compatibility test that code prior to Qt 5.5 linking to
# Qt5::Designer gets the required include directories for using
# the QDesignerCustomWidgetInterface.

cmake_minimum_required(VERSION 2.8.11)
project(test_uiplugin_via_designer)

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(Qt5Designer REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if (NOT CMAKE_VERSION VERSION_LESS 3.0)
  set(CMAKE_HAS_INTERFACE 1)
else()
  set(CMAKE_HAS_INTERFACE 0)
endif()

add_definitions(-DTEST_UIPLUGIN_USAGE_REQUIREMENTS=${CMAKE_HAS_INTERFACE})

add_library(my_designer_plugin my_designer_plugin.cpp)
target_link_libraries(my_designer_plugin Qt5::Designer)
