############################################################################
# CMakeLists.txt
# Copyright (C) 2014  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

cmake_minimum_required(VERSION 3.0)
project(oRTP VERSION 0.27.0)


set(ORTP_MAJOR_VERSION ${PROJECT_VERSION_MAJOR})
set(ORTP_MINOR_VERSION ${PROJECT_VERSION_MINOR})
set(ORTP_MICRO_VERSION ${PROJECT_VERSION_PATCH})
set(ORTP_VERSION ${PROJECT_VERSION})
set(ORTP_SO_VERSION "12") # incremented on June, the 1st 2016. DO NOT INCREMENT BEFORE ORTP 0.28


option(ENABLE_SHARED "Build shared library." YES)
option(ENABLE_STATIC "Build static library." YES)
option(ENABLE_DOC "Enable documentation generation with Doxygen." YES)
option(ENABLE_NTP_TIMESTAMP "Turn on NTP timestamping on packet reception." NO)
option(ENABLE_PERF "Disable costly features to reduce cpu consumtion and increase performance." NO)
option(ENABLE_STRICT "Build with strict compile options." YES)
option(ENABLE_TESTS "Enable compilation of test programs." NO)
option(ENABLE_DEBUG_LOGS "Turn on or off debug level logs." NO)
set(WITH_THREAD_STACK_SIZE "0" CACHE STRING "Set thread stack size (0 is the OS default).")


macro(apply_compile_flags SOURCE_FILES)
	if(${SOURCE_FILES})
		set(options "")
		foreach(a ${ARGV})
			if(STRICT_OPTIONS_${a})
				string(REPLACE ";" " " options_${a} "${STRICT_OPTIONS_${a}}")
				set(options "${options} ${options_${a}}")
			endif()
		endforeach()
		if(options)
			set_source_files_properties(${${SOURCE_FILES}} PROPERTIES COMPILE_FLAGS "${options}")
		endif()
	endif()
endmacro()


include(CheckIncludeFile)
include(CheckFunctionExists)
include(GNUInstallDirs)

if(NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

set(MSVC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/MSVC")
if(MSVC)
	list(APPEND CMAKE_REQUIRED_INCLUDES "${MSVC_INCLUDE_DIR}")
endif()

find_package(Threads)
find_library(LIBM NAMES m)

if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	include("${EP_bctoolbox_CONFIG_DIR}/BcToolboxConfig.cmake")
else()
	find_package(BcToolbox 0.2.0 REQUIRED)
endif()

check_include_file(sys/uio.h HAVE_SYS_UIO_H)
check_include_file(sys/audio.h HAVE_SYS_AUDIO_H)
check_include_file(sys/shm.h HAVE_SYS_SHM_H)
check_include_file(stdatomic.h HAVE_STDATOMIC_H)
check_function_exists(arc4random HAVE_ARC4RANDOM)

include(TestBigEndian)
test_big_endian(WORDS_BIGENDIAN)
if(WORDS_BIGENDIAN)
	set(ORTP_BIGENDIAN 1)
endif()


include_directories(
	include/
	src/
	${CMAKE_CURRENT_BINARY_DIR}
	${BCTOOLBOX_CORE_INCLUDE_DIRS}
)
if(MSVC)
	include_directories(${MSVC_INCLUDE_DIR})
endif()


set(ORTP_CPPFLAGS )
if(ENABLE_STATIC)
	set(ORTP_STATIC 1)
	list(APPEND ORTP_CPPFLAGS "-DORTP_STATIC")
endif()
if(ENABLE_PERF)
	set(PERF 1)
endif()
if(ENABLE_NTP_TIMESTAMP)
	set(ORTP_TIMESTAMP 1)
	list(APPEND ORTP_CPPFLAGS "-DORTP_TIMESTAMP")
endif()
if(ENABLE_DEBUG_LOGS)
	set(ORTP_DEBUG_MODE 1)
endif()
if(CMAKE_USE_PTHREADS_INIT)
	set(ORTP_DEFAULT_THREAD_STACK_SIZE ${WITH_THREAD_STACK_SIZE})
endif()
if(APPLE)
	set(__APPLE_USE_RFC_3542 1)
endif()
set(POSIXTIMER_INTERVAL 10000)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ortp-config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/ortp-config.h)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/ortp-config.h PROPERTIES GENERATED ON)
add_definitions(-DHAVE_CONFIG_H)

set(STRICT_OPTIONS_CPP )
set(STRICT_OPTIONS_C )
if(MSVC)
	if(ENABLE_STRICT)
		list(APPEND STRICT_OPTIONS_CPP "/WX")
	endif()
else()
	list(APPEND STRICT_OPTIONS_CPP "-Wall" "-Wuninitialized")
	list(APPEND STRICT_OPTIONS_C "-Wdeclaration-after-statement" "-Wstrict-prototypes" "-Wno-error=strict-prototypes")
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
		list(APPEND STRICT_OPTIONS_CPP "-Qunused-arguments" "-Wno-array-bounds")
	endif()
	if(APPLE)
		list(APPEND STRICT_OPTIONS_CPP "-Wno-error=unknown-warning-option" "-Qunused-arguments" "-Wno-tautological-compare" "-Wno-unused-function" "-Wno-array-bounds")
	endif()
	if(ENABLE_STRICT)
		list(APPEND STRICT_OPTIONS_CPP "-Werror" "-Wextra" "-Wno-unused-parameter" "-Wno-missing-field-initializers" "-fno-strict-aliasing")
	endif()
endif()
if(STRICT_OPTIONS_CPP)
	list(REMOVE_DUPLICATES STRICT_OPTIONS_CPP)
endif()
if(STRICT_OPTIONS_C)
	list(REMOVE_DUPLICATES STRICT_OPTIONS_C)
endif()


if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	set(EXPORT_TARGETS_NAME "LinphoneBuilder")
else()
	set(EXPORT_TARGETS_NAME "ORTP")
endif()


add_subdirectory(include)
add_subdirectory(src)


if(ENABLE_DOC)
	find_package(Doxygen)
	if(DOXYGEN_FOUND)
		set(srcdir ${CMAKE_CURRENT_SOURCE_DIR})
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ortp.doxygen.in ${CMAKE_CURRENT_BINARY_DIR}/ortp.doxygen)
		file(GLOB DOC_INPUT_FILES
			include/ortp/[^.]*.h
			src/[^.]*.h
			src/[^.]*.c
		)
		add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/doc/html/index.html"
			COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/ortp.doxygen
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/ortp.doxygen ${DOC_INPUT_FILES}
		)
		add_custom_target(ortp-html-doc ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/doc/html/index.html")
		install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doc/html"
			DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/oRTP-${ORTP_VERSION}")
	endif()
endif()




set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${prefix}/bin)
set(libdir ${prefix}/lib)
set(includedir ${prefix}/include)
set(ORTP_PKGCONFIG_VERSION "${ORTP_VERSION}")
set(ORTPDEPS_LIBS )

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ortp.pc.in ${CMAKE_CURRENT_BINARY_DIR}/ortp.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ortp.pc DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")


include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/ORTPConfigVersion.cmake"
	VERSION ${ORTP_VERSION}
	COMPATIBILITY AnyNewerVersion
)
export(EXPORT ${EXPORT_TARGETS_NAME}Targets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/ORTPTargets.cmake"
)
configure_file(ORTPConfig.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/ORTPConfig.cmake"
	@ONLY
)

set(CONFIG_PACKAGE_LOCATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/cmake")
install(EXPORT ${EXPORT_TARGETS_NAME}Targets
	FILE ORTPTargets.cmake
	DESTINATION ${CONFIG_PACKAGE_LOCATION}
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/ORTPConfig.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/ORTPConfigVersion.cmake"
	DESTINATION ${CONFIG_PACKAGE_LOCATION}
)
