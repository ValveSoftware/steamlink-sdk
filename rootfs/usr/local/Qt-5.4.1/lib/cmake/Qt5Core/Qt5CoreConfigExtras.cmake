
if (NOT TARGET Qt5::qmake)
    add_executable(Qt5::qmake IMPORTED)

    set(imported_location "/home/jamesz/work/valve-ro/steamlink-fw/external/qt-everywhere-opensource-src-5.4.1/build/host/bin/qmake")
    _qt5_Core_check_file_exists(${imported_location})

    set_target_properties(Qt5::qmake PROPERTIES
        IMPORTED_LOCATION ${imported_location}
    )
endif()

if (NOT TARGET Qt5::moc)
    add_executable(Qt5::moc IMPORTED)

    set(imported_location "/home/jamesz/work/valve-ro/steamlink-fw/external/qt-everywhere-opensource-src-5.4.1/build/host/bin/moc")
    _qt5_Core_check_file_exists(${imported_location})

    set_target_properties(Qt5::moc PROPERTIES
        IMPORTED_LOCATION ${imported_location}
    )
    # For CMake automoc feature
    get_target_property(QT_MOC_EXECUTABLE Qt5::moc LOCATION)
endif()

if (NOT TARGET Qt5::rcc)
    add_executable(Qt5::rcc IMPORTED)

    set(imported_location "/home/jamesz/work/valve-ro/steamlink-fw/external/qt-everywhere-opensource-src-5.4.1/build/host/bin/rcc")
    _qt5_Core_check_file_exists(${imported_location})

    set_target_properties(Qt5::rcc PROPERTIES
        IMPORTED_LOCATION ${imported_location}
    )
endif()

set(Qt5Core_QMAKE_EXECUTABLE Qt5::qmake)
set(Qt5Core_MOC_EXECUTABLE Qt5::moc)
set(Qt5Core_RCC_EXECUTABLE Qt5::rcc)

set_property(TARGET Qt5::Core PROPERTY INTERFACE_QT_MAJOR_VERSION 5)
set_property(TARGET Qt5::Core PROPERTY INTERFACE_QT_COORD_TYPE double)
set_property(TARGET Qt5::Core APPEND PROPERTY
  COMPATIBLE_INTERFACE_STRING QT_MAJOR_VERSION QT_COORD_TYPE
)

include("${CMAKE_CURRENT_LIST_DIR}/Qt5CoreConfigExtrasMkspecDir.cmake")

foreach(_dir ${_qt5_corelib_extra_includes})
    _qt5_Core_check_file_exists(${_dir})
endforeach()

list(APPEND Qt5Core_INCLUDE_DIRS ${_qt5_corelib_extra_includes})
set_property(TARGET Qt5::Core APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${_qt5_corelib_extra_includes})
set(_qt5_corelib_extra_includes)



set(Qt5_DISABLED_FEATURES
    cups
    imageformat-jpeg
)

set_property(TARGET Qt5::Core APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG>)

set(QT_VISIBILITY_AVAILABLE "True")



get_filename_component(_Qt5CoreConfigDir ${CMAKE_CURRENT_LIST_FILE} PATH)

set(_Qt5CTestMacros "${_Qt5CoreConfigDir}/Qt5CTestMacros.cmake")

_qt5_Core_check_file_exists(${_Qt5CTestMacros})
