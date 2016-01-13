
cmake_minimum_required(VERSION 2.8)

project(test_translation_macros)

find_package(Qt5Core 5.0.0 REQUIRED)
find_package(Qt5LinguistTools 5.0.0 REQUIRED)

include_directories(
  ${Qt5Core_INCLUDE_DIRS}
)

add_definitions(${Qt5Core_DEFINITIONS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Core_EXECUTABLE_COMPILE_FLAGS}")

set(my_srcs myi18nobject.cpp)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/some_dir")

qt5_create_translation(qm_files ${my_srcs} myobject_de.ts)

add_executable(myobject ${my_srcs} ${qm_files})
target_link_libraries(myobject ${Qt5Core_LIBRARIES})
