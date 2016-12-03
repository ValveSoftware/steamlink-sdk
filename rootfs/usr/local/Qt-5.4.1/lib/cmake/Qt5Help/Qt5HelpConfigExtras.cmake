
if (NOT TARGET Qt5::qcollectiongenerator)
    add_executable(Qt5::qcollectiongenerator IMPORTED)

    set(imported_location "/home/jamesz/work/valve-ro/steamlink-fw/external/qt-everywhere-opensource-src-5.4.1/build/host/bin/qcollectiongenerator")
    _qt5_Help_check_file_exists(${imported_location})

    set_target_properties(Qt5::qcollectiongenerator PROPERTIES
        IMPORTED_LOCATION ${imported_location}
    )
endif()
