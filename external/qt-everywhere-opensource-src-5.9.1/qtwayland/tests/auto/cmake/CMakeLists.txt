
cmake_minimum_required(VERSION 2.8)

project(qmake_cmake_files)

enable_testing()

find_package(Qt5Core REQUIRED)
set(Qt5_MODULE_TEST_DEPENDS Quick)

include("${_Qt5CTestMacros}")

test_module_includes(
  WaylandCompositor QWaylandBufferRef
)

# Can't test in `test_module_includes`, WaylandClient has no public headers
expect_pass(test_waylandclient)
