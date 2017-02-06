
cmake_minimum_required(VERSION 2.8)

project(qmake_cmake_files)

enable_testing()

find_package(Qt5Core REQUIRED)

include("${_Qt5CTestMacros}")

# Qt5Qml is an optional dependency of QWebChannel headers. Make sure we find it
# in the test.
set(Qt5_MODULE_TEST_DEPENDS Qml)

test_module_includes(
    WebChannel QWebChannel
)
