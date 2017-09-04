#
# Copyright (C) 2015 Ford Motor Company
# Contact: https://www.qt.io/licensing/
#
# This file is part of the QtRemoteObjects module of the Qt Toolkit.
#
# $QT_BEGIN_LICENSE:LGPL21$
# Commercial License Usage
# Licensees holding valid commercial Qt licenses may use this file in
# accordance with the commercial license agreement provided with the
# Software or, alternatively, in accordance with the terms contained in
# a written agreement between you and The Qt Company. For licensing terms
# and conditions see http://www.qt.io/terms-conditions. For further
# information use the contact form at http://www.qt.io/contact-us.
#
# GNU Lesser General Public License Usage
# Alternatively, this file may be used under the terms of the GNU Lesser
# General Public License version 2.1 or version 3 as published by the Free
# Software Foundation and appearing in the file LICENSE.LGPLv21 and
# LICENSE.LGPLv3 included in the packaging of this file. Please review the
# following information to ensure the GNU Lesser General Public License
# requirements will be met: https://www.gnu.org/licenses/lgpl.html and
# http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
#
# As a special exception, The Qt Company gives you certain additional
# rights. These rights are described in The Qt Company LGPL Exception
# version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
#
# $QT_END_LICENSE$

# Macros
# =========
#
# In some cases it can be necessary or useful to invoke the Qt build tools in a
# more-manual way. Several macros are available to add targets for such uses.
#
# ::
#
#   macro qt5_generate_repc(outfiles infile outputtype)
#         out: outfiles
#         in: infile outputtype
#         create C++ code from a list of .rep files. Per-directory preprocessor
#         definitions are also added.
#         infile should be a replicant template file (.rep)
#         outputtype specifies output file type, it can be one of SOURCE|REPLICA
#         Example usage: qt5_generate_repc(LIB_SRCS interface.rep SOURCE)
#           for generating interface_source.h and adding it to LIB_SRCS

if(NOT Qt5RemoteObjects_REPC_EXECUTABLE)
    message(FATAL_ERROR "repc executable not found -- Check installation.")
endif()

macro(qt5_generate_repc outfiles infile outputtype)
    # get include dirs and flags
    get_filename_component(abs_infile ${infile} ABSOLUTE)
    get_filename_component(infile_name "${infile}" NAME)
    string(REPLACE ".rep" "" _infile_base ${infile_name})
    if(${outputtype} STREQUAL "SOURCE")
        set(_outfile_base "rep_${_infile_base}_source")
        set(_repc_args -o source)
    else()
        set(_outfile_base "rep_${_infile_base}_replica")
        set(_repc_args -o replica)
    endif()
    set(_outfile_header "${CMAKE_CURRENT_BINARY_DIR}/${_outfile_base}.h")
    add_custom_command(OUTPUT ${_outfile_header}
        DEPENDS ${abs_infile}
        COMMAND ${Qt5RemoteObjects_REPC_EXECUTABLE} ${abs_infile} ${_repc_args} ${_outfile_header}
        VERBATIM)
    set_source_files_properties(${_outfile_header} PROPERTIES GENERATED TRUE)

    qt5_get_moc_flags(_moc_flags)
    # Make sure we get the compiler flags from the Qt5::RemoteObjects target (for includes)
    # (code adapted from QT5_GET_MOC_FLAGS)
    foreach(_current ${Qt5RemoteObjects_INCLUDE_DIRS})
        if("${_current}" MATCHES "\\.framework/?$")
            string(REGEX REPLACE "/[^/]+\\.framework" "" framework_path "${_current}")
            set(_moc_flags ${_moc_flags} "-F${framework_path}")
        else()
            set(_moc_flags ${_moc_flags} "-I${_current}")
        endif()
    endforeach()

    set(_moc_outfile "${CMAKE_CURRENT_BINARY_DIR}/moc_${_outfile_base}.cpp")
    qt5_create_moc_command(${_outfile_header} ${_moc_outfile} "${_moc_flags}" "" "" "")
    list(APPEND ${outfiles} "${_outfile_header}" ${_moc_outfile})
endmacro()
