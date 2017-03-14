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

set(LIBS ${BCTOOLBOX_CORE_LIBRARIES})
if(NOT ANDROID)
	list(APPEND LIBS ${CMAKE_THREAD_LIBS_INIT})
endif()
if(HAVE_RT)
	list(APPEND LIBS rt)
endif()
if(LIBM)
	list(APPEND LIBS ${LIBM})
endif()

set(ORTP_SOURCE_FILES_C
	avprofile.c
	b64.c
	event.c
	extremum.c
	jitterctl.c
	logging.c
	netsim.c
	ortp.c
	payloadtype.c
	port.c
	posixtimer.c
	rtcp.c
	rtcp_fb.c
	rtcp_xr.c
	rtcpparse.c
	rtpparse.c
	rtpprofile.c
	rtpsession.c
	rtpsession_inet.c
	rtpsignaltable.c
	rtptimer.c
	scheduler.c
	sessionset.c
	str_utils.c
	telephonyevents.c
	utils.c
)
set(ORTP_SOURCE_FILES_CXX )

if(WIN32)
	list(APPEND ORTP_SOURCE_FILES_C dll_entry.c)
	if(CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone")
		list(APPEND ORTP_SOURCE_FILES_CXX winrttimer.cpp winrttimer.h)
		set_source_files_properties(winrttimer.cpp PROPERTIES COMPILE_FLAGS "/ZW /AI\$(WindowsSDK_MetadataPath)")
	endif()
endif()

if(WIN32)
	add_definitions(-DWINDOWS_NATIVE)
	if(NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
		list(APPEND LIBS ws2_32)
	endif()
	if(NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone" AND NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
		list(APPEND LIBS delayimp Winmm Qwave)
	endif()
endif()

apply_compile_flags(ORTP_SOURCE_FILES_C "CPP" "C")
apply_compile_flags(ORTP_SOURCE_FILES_CXX "CPP" "CXX")

if(ENABLE_STATIC)
	add_library(ortp-static STATIC ${ORTP_HEADER_FILES} ${ORTP_SOURCE_FILES_C} ${ORTP_SOURCE_FILES_CXX})
	set_target_properties(ortp-static PROPERTIES OUTPUT_NAME ortp)
	target_compile_definitions(ortp-static PRIVATE "ORTP_EXPORTS")
	target_link_libraries(ortp-static ${LIBS})
	install(TARGETS ortp-static EXPORT ${EXPORT_TARGETS_NAME}Targets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
if(ENABLE_SHARED)
	add_library(ortp SHARED ${ORTP_HEADER_FILES} ${ORTP_SOURCE_FILES_C} ${ORTP_SOURCE_FILES_CXX})
	if(ANDROID)
		set_target_properties(ortp PROPERTIES OUTPUT_NAME "ortp-${CMAKE_SYSTEM_PROCESSOR}")
	else()
		# Do not version shared library on Android
		set_target_properties(ortp PROPERTIES SOVERSION ${ORTP_SO_VERSION})
	endif()
	if(WIN32)
		target_compile_options(ortp PRIVATE "/DELAYLOAD:Qwave.dll")
	endif()
	target_compile_definitions(ortp PRIVATE "ORTP_EXPORTS")
	target_link_libraries(ortp PRIVATE ${LIBS})
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/ortp.pdb
				DESTINATION ${CMAKE_INSTALL_BINDIR}
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif()
	install(TARGETS ortp EXPORT ${EXPORT_TARGETS_NAME}Targets
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()

if(ENABLE_TESTS AND NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone" AND NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	add_subdirectory(tests)
endif()
