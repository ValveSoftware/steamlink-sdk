cmake_minimum_required(VERSION 2.8)

project(test_add_translation_macro)

find_package(Qt5Core 5.0.0 REQUIRED)
find_package(Qt5LinguistTools 5.0.0 REQUIRED)

include_directories(
  ${Qt5Core_INCLUDE_DIRS}
)

add_definitions(${Qt5Core_DEFINITIONS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Core_EXECUTABLE_COMPILE_FLAGS}")

set(my_srcs myi18nobject.cpp)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/some_dir")

qt5_add_translation(qm_files myobject_de.ts)

add_executable(test_add_translation ${my_srcs} ${qm_files})
target_link_libraries(test_add_translation ${Qt5Core_LIBRARIES})
