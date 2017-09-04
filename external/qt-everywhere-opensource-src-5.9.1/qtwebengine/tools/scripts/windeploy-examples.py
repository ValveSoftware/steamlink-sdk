#!/usr/bin/env python

#############################################################################
#
# Copyright (C) 2015 The Qt Company Ltd.
# Contact: http://www.qt.io/licensing/
#
# This file is part of the QtWebEngine module of the Qt Toolkit.
#
# $QT_BEGIN_LICENSE:LGPL$
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
# General Public License version 2.1 as published by the Free Software
# Foundation and appearing in the file LICENSE.LGPL included in the
# packaging of this file. Please review the following information to
# ensure the GNU Lesser General Public License version 2.1 requirements
# will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
#
# As a special exception, The Qt Company gives you certain additional
# rights. These rights are described in The Qt Company LGPL Exception
# version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
#
# GNU General Public License Usage
# Alternatively, this file may be used under the terms of the GNU
# General Public License version 3.0 as published by the Free Software
# Foundation and appearing in the file LICENSE.GPL included in the
# packaging of this file. Please review the following information to
# ensure the GNU General Public License version 3.0 requirements will be
# met: http://www.gnu.org/copyleft/gpl.html.
#
#
# $QT_END_LICENSE$
#
#############################################################################

import argparse
import os
import re
import subprocess
import shutil
import sys


class Api:
    QUICK = "webengine"
    WIDGET = "webenginewidgets"


class Mode:
    RELEASE = "release"
    DEBUG = "debug"


def is_windows():
    return os.name == "nt"


class ArgManager(object):
    def __init__(self):
        try:
            self.__args = self.__parse()
        except:
            raise

        self.build_dir = self.__args.build_dir
        self.src_dir = self.__args.src_dir
        self.example_filter = re.compile("%s-%s-%s" % (self.__api_filter(), self.__name_filter(), self.__mode_filter()))
        self.list_examples = self.__args.list_examples
        self.force = self.__args.force
        self.verbose = self.__args.verbose
        self.out_dir = self.__args.out_dir

    def __parse(self):
        ap = argparse.ArgumentParser(description="Deploy QtWebEngine example binaries on Windows.")
        ap.add_argument("--release", dest="release", action="store_true", default=False,
                        help="Deploy release binaries only")
        ap.add_argument("--debug", dest="debug", action="store_true", default=False,
                        help="Deploy debug binaries only")
        ap.add_argument("--quick", dest="quick", action="store_true", default=False,
                        help="Deploy quick examples only")
        ap.add_argument("--widget", dest="widget", action="store_true", default=False,
                        help="Deploy widget examples only")
        ap.add_argument("-e", "--examples", dest="examples", nargs="+", action="store", default=".*",
                        help="Select example to deploy")
        ap.add_argument("-l", "--list-examples", dest="list_examples", action="store_true", default=False,
                        help="List available examples")
        ap.add_argument("-f", "--force", dest="force", action="store_true", default=False,
                        help="Force to overwrite existing files")
        ap.add_argument("-v", "--verbose", dest="verbose", action="store_true", default=False,
                        help="Print windeployqt output")
        ap.add_argument("--src-dir", dest="src_dir", action="store", type=self.__validate_src_dir, default=None,
                        help="Specify path of Qt sources. It is used for finding QML files of the example"
                             "and scanning for QML imports")
        ap.add_argument("--build-dir", dest="build_dir", action="store", type=self.__validate_build_dir, default=None,
                        help="Specify path of the Qt binaries. It is used for finding qmake.exe, windeployqt.exe and"
                             "binaries of the examples. It is not necessary to set if qmake.exe is set in the path")
        ap.add_argument("-o", "--out-dir", dest="out_dir", action="store", default=os.getcwd(),
                        help="Specify path for the deployed examples. If it is not set"
                             "current working directory is used")
        return ap.parse_args()

    def __validate_src_dir(self, src_dir):
        if not os.path.exists(src_dir):
            raise OSError("The specified Qt source directory does not exist: %s" % src_dir)

        qtwebengine_dir = src_dir
        # Accept Qt top level source directory too
        if os.path.exists(os.path.join(src_dir, "qtwebengine")):
            qtwebengine_dir = os.path.join(src_dir, "qtwebengine")

        examples_dir = os.path.join(qtwebengine_dir, "examples")
        must_have_paths = [
            os.path.join(examples_dir, "examples.pro"),
            os.path.join(examples_dir, Api.QUICK),
            os.path.join(examples_dir, Api.WIDGET),
        ]

        # Check whether src_dir is the proper QtWebEngine source directory
        for p in must_have_paths:
            if not os.path.exists(p):
                raise OSError("The specified Qt source directory is invalid: %s" % src_dir)

        return src_dir

    def __validate_build_dir(self, build_dir):
        if not os.path.exists(build_dir):
            raise OSError("The specified Qt build directory does not exist: %s" % build_dir)

        # Accept QtWebEngine build directory too
        if os.path.basename(build_dir) == "qtwebengine":
            build_dir = os.path.abspath(os.path.join(build_dir, ".."))

        # Attempt to support custom build directories
        if os.path.exists(os.path.join(build_dir, "bin", "qmake.exe")):
            return build_dir

        # Check existence of qtbase/bin/qmake.exe
        qtbase_dir = os.path.join(build_dir, "qtbase")
        if not os.path.exists(os.path.join(qtbase_dir, "bin", "qmake.exe")):
            raise OSError("Program 'qmake.exe' cannot be found in the specified Qt build directory: %s" % build_dir)

        return qtbase_dir

    def __mode_filter(self):
        if self.__args.release and not self.__args.debug:
            return Mode.RELEASE
        if not self.__args.release and self.__args.debug:
            return Mode.DEBUG
        return ".*"

    def __api_filter(self):
        if self.__args.quick and not self.__args.widget:
            return Api.QUICK
        if not self.__args.quick and self.__args.widget:
            return Api.WIDGET
        return ".*"

    def __name_filter(self):
        alt_list = []

        examples = self.__args.examples
        if not isinstance(examples, list):
            examples = [examples]

        for e in examples:
            if "," in e:
                alt_list.extend(e.split(","))
            else:
                alt_list.append(e)

        return "|".join(alt_list)


class QtHelper(object):
    def __init__(self, build_path=None, src_path=None):
        self.__build_path = build_path if build_path is not None else ""
        self.__src_path = src_path if src_path is not None else ""
        self.__query = {}
        self.__angle = None

        self.__qmake = "qmake.exe"
        self.__windeployqt = "windeployqt.exe"
        if build_path:
            self.__qmake = os.path.join(self.__build_path, "bin", self.__qmake)
            self.__windeployqt = os.path.join(self.__build_path, "bin", self.__windeployqt)

        try:
            program = self.__qmake
            subprocess.check_output([program, "-v"])
            program = self.__windeployqt
            subprocess.check_output([program, "-h"])
        except OSError as e:
            raise OSError("Program '%s' cannot be executed\n%s" % (program, e))
        except:
            raise

        self.__query = self.get_query()
        self.__build_path = self.get_build_path()
        self.__src_path = self.get_src_path()
        self.__angle = self.has_angle()

    def get_query(self):
        if self.__query:
            return self.__query

        qmake_output = subprocess.check_output([self.__qmake, "-query"]).split("\r\n")
        query = {}
        for line in qmake_output:
            entry = line.split(":", 1)
            if len(entry) != 2:
                continue
            query[entry[0]] = entry[1]
        return query

    def get_build_path(self):
        return os.path.abspath(os.path.join(self.__query["QT_INSTALL_PREFIX"], ".."))

    def get_src_path(self):
        if self.__src_path:
            return self.__src_path

        if "QT_INSTALL_PREFIX/src" in self.__query:
            return os.path.abspath(os.path.join(self.__query["QT_INSTALL_PREFIX/src"], ".."))

        return self.__build_path

    def get_windeployqt(self):
        return self.__windeployqt

    def has_angle(self):
        if self.__angle:
            return self.__angle

        qconfig_pri_path = os.path.abspath(os.path.join(self.__query["QT_HOST_PREFIX"], "mkspecs", "qconfig.pri"))
        print(qconfig_pri_path)
        if not os.path.exists(qconfig_pri_path):
            sys.stderr.write("Configuration file qconfig.pri cannot be found. Fallback to desktop GL mode.\n")
            return False

        qt_config = []

        qconfig_pri = open(qconfig_pri_path, "r")
        for line in qconfig_pri:
            if line.startswith("QT_CONFIG +="):
                qt_config = re.match("^QT_CONFIG \+= (.+)$", line).group(1).split(" ")
        qconfig_pri.close()

        return "angle" in qt_config

    def collect_examples(self):
        examples = []

        for api in [Api.QUICK, Api.WIDGET]:
            examples_root_dir_path = os.path.join(self.__build_path, "qtwebengine", "examples", api)
            if not os.path.exists(examples_root_dir_path):
                continue

            for example in os.listdir(examples_root_dir_path):
                example_dir_path = os.path.join(examples_root_dir_path, example)
                if not os.path.exists(example_dir_path):
                    continue

                example_src_path = os.path.join(self.__src_path, "qtwebengine", "examples", api, example)

                for mode in [Mode.RELEASE, Mode.DEBUG]:
                    example_exe_path = os.path.join(example_dir_path, mode, example + ".exe")
                    if not os.path.exists(example_exe_path):
                        continue
                    examples.append(Example(example, api, mode, self.__angle, example_exe_path, example_src_path))

        return examples


class Example(str):
    def __new__(cls, example, api, mode, angle, exe_path, src_path):
        obj = str.__new__(cls, "%s-%s-%s" % (api, example, mode))
        return obj

    def __init__(self, example, api, mode, angle, exe_path, src_path):
        super(Example, self).__init__("%s-%s-%s" % (api, example, mode))
        self.__name = example
        self.__api = api
        self.__mode = mode
        self.__angle = angle
        self.__exe_path = exe_path
        self.__src_path = src_path

    def get_deploy_params(self, mode, force):
        deploy_params = []
        deploy_params.append("--%s" % mode)
        deploy_params.append("--compiler-runtime")

        if self.__angle:
            deploy_params.append("--angle")

        if force:
            deploy_params.append("--force")

        if self.__api is Api.QUICK:
            deploy_params.append("--qmldir")
            deploy_params.append(self.__src_path)

        if self.__mode is Mode.DEBUG:
            deploy_params.append("--pdb")

        return deploy_params

    def name(self):
        return self.__name

    def deploy(self, qt, dest_path, force=False, verbose=False):
        src_path = os.path.dirname(self.__exe_path)
        for f in os.listdir(src_path):
            shutil.copy(os.path.join(src_path, f), dest_path)

        deploy_command = []
        deploy_command.append(qt.get_windeployqt())
        # Debug executables also need the release libraries
        deploy_command.extend(self.get_deploy_params(Mode.RELEASE, force))
        deploy_command.append(dest_path)

        if verbose:
            print("%s" % " ".join(deploy_command))

        out = None if verbose else open(os.devnull, "w")
        exit_code = subprocess.call(deploy_command, stdout=out)

        if self.__mode is Mode.DEBUG and not exit_code:
            param_release = "--%s" % Mode.RELEASE
            param_debug = "--%s" % Mode.DEBUG
            deploy_command = map(lambda item: param_debug if item == param_release else item, deploy_command)

            if verbose:
                print("%s" % " ".join(deploy_command))

            exit_code = subprocess.call(deploy_command, stdout=out)

        if out is not None:
            out.close()

        return exit_code


def main():
    try:
        args = ArgManager()

        if not is_windows():
            raise OSError("This script works on Windows only\n")

        qt = QtHelper(args.build_dir, args.src_dir)
    except Exception as e:
        sys.stderr.write(str(e))
        exit(1)

    example_list = filter((lambda e: args.example_filter.match(e)), qt.collect_examples())
    if not example_list:
        print("There is no example that fulfills the requirements")
        return

    for example in example_list:
        if args.list_examples:
            print(example.name())
            continue

        print("Deploying %s ..." % example.name())
        dest_path = os.path.abspath(os.path.join(args.out_dir, example.name()))
        if not os.path.exists(dest_path):
            os.makedirs(dest_path)
        elif os.listdir(dest_path) and not args.force:
            sys.stderr.write("Destination directory is not empty: %s\n" % dest_path)
            sys.stderr.write("Skip deploying %s\n" % example.name())
            continue

        exit_code = example.deploy(qt, dest_path, args.force, args.verbose)
        if exit_code:
            sys.stderr.write("Deploy of example '%s' has failed: %s\n" % (example.name(), exit_code))
            exit(exit_code)

        print("Example '%s' has been successfully deployed at %s" % (example.name(), dest_path))

    return

if __name__ == "__main__":
    main()
