#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SDL3_ttf::SDL3_ttf-shared" for configuration "Release"
set_property(TARGET SDL3_ttf::SDL3_ttf-shared APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(SDL3_ttf::SDL3_ttf-shared PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "SDL3::SDL3-shared"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libSDL3_ttf.so.0.0.0"
  IMPORTED_SONAME_RELEASE "libSDL3_ttf.so.0"
  )

list(APPEND _cmake_import_check_targets SDL3_ttf::SDL3_ttf-shared )
list(APPEND _cmake_import_check_files_for_SDL3_ttf::SDL3_ttf-shared "${_IMPORT_PREFIX}/lib/libSDL3_ttf.so.0.0.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
