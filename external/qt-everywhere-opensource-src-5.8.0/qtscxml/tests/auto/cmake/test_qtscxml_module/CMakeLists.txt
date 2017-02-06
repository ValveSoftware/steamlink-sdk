cmake_minimum_required(VERSION 2.8)

project(test_qtscxml_module)

find_package(Qt5Scxml REQUIRED)

set(MAIN_SRCS main.cpp)
qt5_add_statecharts(MAIN_SRCS
    ../../compiled/connection.scxml

    # unused, just for testing whether it's possible to pass multiple files
    ../../compiled/topmachine.scxml
)
add_executable(mainapp ${MAIN_SRCS})
target_include_directories(mainapp PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(mainapp Qt5::Scxml)
