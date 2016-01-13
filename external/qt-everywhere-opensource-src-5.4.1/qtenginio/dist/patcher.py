#!/usr/bin/python
# -*- coding: utf-8 -*-

# Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
# Contact: http://engin.io
#
# You may use this file under the terms of the 3-clause BSD license.
# See the file LICENSE from this package for details.
#

from __future__ import print_function
import os
import shutil
import subprocess
import sys

script_path = os.path.abspath(os.path.dirname(sys.argv[0]))
os.chdir(script_path)
qmake = "bin/qmake"
otool = "otool"
install_name_tool = "install_name_tool"

cwd = os.getcwd()
print(cwd)

qt_install_placeholder = "<__QT__INSTALL__PREFIX__>"
qt_install_prefix = subprocess.check_output([qmake, "-query", "QT_INSTALL_PREFIX"]).strip().decode('utf-8')

if sys.platform == "darwin" or sys.platform.startswith("linux"):
    filenames = ['mkspecs/modules/qt_lib_enginio.pri',
                 'lib/Enginio.framework/Enginio_debug.prl',
                 'lib/Enginio.framework/Enginio.prl',
                 'lib/libEnginio.prl',
                 'lib/pkgconfig/Enginio.pc',
                 'lib/libEnginio.la',
                 'lib/Enginio.la',
                 'lib/Enginio_debug.la']

    for f in filenames:
        if os.path.exists(f):
            with open(f, "r+") as f_handle:
                lines = f_handle.readlines()
                f_handle.seek(0)
                f_handle.truncate()
                for line in lines:
                    modline = line
                    if qt_install_placeholder in line:
                        modline = line.replace(qt_install_placeholder, qt_install_prefix)
                    f_handle.write(modline)

if not sys.platform == "darwin":
    exit(0)

plugin_lib = "qml/Enginio/libenginioplugin.dylib"
plugin_debug_lib = "qml/Enginio/libenginioplugin_debug.dylib"

install_path = subprocess.check_output([otool, "-D", "lib/Enginio.framework/Enginio"]).split().pop()
rpath = install_path.replace(qt_install_placeholder, qt_install_prefix)
enginio_lib = install_path.replace(qt_install_placeholder, '.')

subprocess.check_call([install_name_tool, "-id", rpath, enginio_lib])
subprocess.check_call([install_name_tool, "-id", rpath + "_debug", enginio_lib + "_debug"])

rpath_libs = subprocess.check_output([otool, "-L", enginio_lib]).strip().split('\n')
rpath_libs.extend(subprocess.check_output([otool, "-L", plugin_lib]).strip().split('\n'))
rpath_libs.extend(subprocess.check_output([otool, "-L", plugin_debug_lib]).strip().split('\n'))

seen = set()
for line in rpath_libs:
    lib = line.split()[0].strip()
    if lib not in seen and lib.startswith(qt_install_placeholder):
        print("# " + lib)
        subprocess.check_call([install_name_tool, "-change", lib, lib.replace(qt_install_placeholder, qt_install_prefix), enginio_lib])
        subprocess.check_call([install_name_tool, "-change", lib, lib.replace(qt_install_placeholder, qt_install_prefix), enginio_lib + "_debug"])
        subprocess.check_call([install_name_tool, "-change", lib, lib.replace(qt_install_placeholder, qt_install_prefix), plugin_lib])
        subprocess.check_call([install_name_tool, "-change", lib, lib.replace(qt_install_placeholder, qt_install_prefix), plugin_debug_lib])
    seen.add(lib)

os.chdir(cwd)

