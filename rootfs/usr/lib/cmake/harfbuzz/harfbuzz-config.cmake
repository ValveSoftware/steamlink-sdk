# Set these variables so that the `${prefix}/lib` expands to something we can
# remove.
set(_harfbuzz_remove_string "REMOVE_ME")
set(exec_prefix "${_harfbuzz_remove_string}")
set(prefix "${_harfbuzz_remove_string}")

# Compute the installation prefix by stripping components from our current
# location.
get_filename_component(_harfbuzz_prefix "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)
get_filename_component(_harfbuzz_prefix "${_harfbuzz_prefix}" DIRECTORY)
set(_harfbuzz_libdir "${exec_prefix}/lib")
string(REPLACE "${_harfbuzz_remove_string}/" "" _harfbuzz_libdir "${_harfbuzz_libdir}")
set(_harfbuzz_libdir_iter "${_harfbuzz_libdir}")
while (_harfbuzz_libdir_iter)
  set(_harfbuzz_libdir_prev_iter "${_harfbuzz_libdir_iter}")
  get_filename_component(_harfbuzz_libdir_iter "${_harfbuzz_libdir_iter}" DIRECTORY)
  if (_harfbuzz_libdir_prev_iter STREQUAL _harfbuzz_libdir_iter)
    break()
  endif ()
  get_filename_component(_harfbuzz_prefix "${_harfbuzz_prefix}" DIRECTORY)
endwhile ()
unset(_harfbuzz_libdir_iter)

# Get the include subdir.
set(_harfbuzz_includedir "${prefix}/include")
string(REPLACE "${_harfbuzz_remove_string}/" "" _harfbuzz_includedir "${_harfbuzz_includedir}")

# Extract version information from libtool.
set(_harfbuzz_version_info "20800:0:20800")
string(REPLACE ":" ";" _harfbuzz_version_info "${_harfbuzz_version_info}")
list(GET _harfbuzz_version_info 0
  _harfbuzz_current)
list(GET _harfbuzz_version_info 1
  _harfbuzz_revision)
list(GET _harfbuzz_version_info 2
  _harfbuzz_age)
unset(_harfbuzz_version_info)

if (APPLE)
  set(_harfbuzz_lib_suffix ".0${CMAKE_SHARED_LIBRARY_SUFFIX}")
elseif (UNIX)
  set(_harfbuzz_lib_suffix "${CMAKE_SHARED_LIBRARY_SUFFIX}.0.${_harfbuzz_current}.${_harfbuzz_revision}")
else ()
  # Unsupported.
  set(harfbuzz_FOUND 0)
endif ()

# Add the libraries.
add_library(harfbuzz::harfbuzz SHARED IMPORTED)
set_target_properties(harfbuzz::harfbuzz PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${_harfbuzz_prefix}/${_harfbuzz_includedir}/harfbuzz"
  IMPORTED_LOCATION "${_harfbuzz_prefix}/${_harfbuzz_libdir}/libharfbuzz${_harfbuzz_lib_suffix}")

add_library(harfbuzz::icu SHARED IMPORTED)
set_target_properties(harfbuzz::icu PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${_harfbuzz_prefix}/${_harfbuzz_includedir}/harfbuzz"
  INTERFACE_LINK_LIBRARIES "harfbuzz::harfbuzz"
  IMPORTED_LOCATION "${_harfbuzz_prefix}/${_harfbuzz_libdir}/libharfbuzz-icu${_harfbuzz_lib_suffix}")

add_library(harfbuzz::subset SHARED IMPORTED)
set_target_properties(harfbuzz::subset PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${_harfbuzz_prefix}/${_harfbuzz_includedir}/harfbuzz"
  INTERFACE_LINK_LIBRARIES "harfbuzz::harfbuzz"
  IMPORTED_LOCATION "${_harfbuzz_prefix}/${_harfbuzz_libdir}/libharfbuzz-subset${_harfbuzz_lib_suffix}")

# Only add the gobject library if it was built.
set(_harfbuzz_have_gobject "false")
if (_harfbuzz_have_gobject)
  add_library(harfbuzz::gobject SHARED IMPORTED)
  set_target_properties(harfbuzz::gobject PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_harfbuzz_prefix}/${_harfbuzz_includedir}/harfbuzz"
    INTERFACE_LINK_LIBRARIES "harfbuzz::harfbuzz"
    IMPORTED_LOCATION "${_harfbuzz_prefix}/${_harfbuzz_libdir}/libharfbuzz-gobject${_harfbuzz_lib_suffix}")
endif ()

# Clean out variables we used in our scope.
unset(_harfbuzz_lib_suffix)
unset(_harfbuzz_current)
unset(_harfbuzz_revision)
unset(_harfbuzz_age)
unset(_harfbuzz_includedir)
unset(_harfbuzz_libdir)
unset(_harfbuzz_prefix)
unset(exec_prefix)
unset(prefix)
unset(_harfbuzz_remove_string)
