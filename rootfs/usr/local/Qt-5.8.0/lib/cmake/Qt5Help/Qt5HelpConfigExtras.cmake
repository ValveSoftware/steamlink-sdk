
if (NOT TARGET Qt5::qcollectiongenerator)
    add_executable(Qt5::qcollectiongenerator IMPORTED)

    set(imported_location "/home/saml/dev/steamlink/firmware/external/qt-everywhere-opensource-src-5.8.0/build/host/bin/qcollectiongenerator")
    _qt5_Help_check_file_exists(${imported_location})

    set_target_properties(Qt5::qcollectiongenerator PROPERTIES
        IMPORTED_LOCATION ${imported_location}
    )
endif()

if (NOT TARGET Qt5::qhelpgenerator)
    add_executable(Qt5::qhelpgenerator IMPORTED)

    set(imported_location "/home/saml/dev/steamlink/firmware/external/qt-everywhere-opensource-src-5.8.0/build/host/bin/qhelpgenerator")
    _qt5_Help_check_file_exists(${imported_location})

    set_target_properties(Qt5::qhelpgenerator PROPERTIES
        IMPORTED_LOCATION ${imported_location}
    )
endif()
