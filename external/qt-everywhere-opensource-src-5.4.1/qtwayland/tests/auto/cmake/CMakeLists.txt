
cmake_minimum_required(VERSION 2.8)

project(qmake_cmake_files)

enable_testing()

find_package(Qt5Core REQUIRED)
set(Qt5_MODULE_TEST_DEPENDS Quick)

include("${_Qt5CTestMacros}")

test_module_includes(
  Compositor QWaylandBufferRef
)
