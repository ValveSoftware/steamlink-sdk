#!/usr/bin/env python

#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the QtWebEngine module of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:GPL-EXCEPT$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3 as published by the Free Software
## Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################

import glob
import os
import re
import shutil
import subprocess
import sys
import json
import urllib2
import git_submodule as GitSubmodule

chromium_version = '53.0.2785.148'
chromium_branch = '2785'
ninja_version = 'v1.7.1'

json_url = 'http://omahaproxy.appspot.com/all.json'

qtwebengine_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
snapshot_src_dir = os.path.abspath(os.path.join(qtwebengine_root, 'src/3rdparty'))
upstream_src_dir = os.path.abspath(snapshot_src_dir + '_upstream')

submodule_blacklist = [
    'third_party/WebKit/LayoutTests/w3c/csswg-test'
    , 'third_party/WebKit/LayoutTests/w3c/web-platform-tests'
    , 'third_party/jsoncpp/source'
    , 'chrome/tools/test/reference_build/chrome_mac'
    , 'chrome/tools/test/reference_build/chrome_linux'
    , 'chrome/tools/test/reference_build/chrome_win'
    ]

sys.path.append(os.path.join(qtwebengine_root, 'tools', 'scripts'))

def currentVersion():
    return chromium_version

def currentNinjaVersion():
    return ninja_version

def readReleaseChannels():
    response = urllib2.urlopen(json_url)
    raw_json = response.read().strip()
    data = json.loads(raw_json)
    channels = {}

    for obj in data:
        os = obj['os']
        channels[os] = []
        for ver in obj['versions']:
            channels[os].append({ 'channel': ver['channel'], 'version': ver['version'], 'branch': ver['true_branch'] })
    return channels

def readSubmodules():
    git_deps = subprocess.check_output(['git', 'show', chromium_version +':DEPS'])

    parser = GitSubmodule.DEPSParser()
    git_submodules = parser.parse(git_deps)

    submodule_dict = {}

    for sub in git_submodules:
        submodule_dict[sub.path] = sub

    # Add buildtools submodules
    buildtools_deps_file_path = "buildtools/DEPS"
    if (os.path.isfile(buildtools_deps_file_path)):
        with open(buildtools_deps_file_path, 'r') as buildtools_deps_file:
            buildtools_deps = buildtools_deps_file.read()
            if buildtools_deps:
                buildtools_parser = GitSubmodule.DEPSParser()
                buildtools_parser.topmost_supermodule_path_prefix = './buildtools/'
                buildtools_submodules = buildtools_parser.parse(buildtools_deps)
                for sub in buildtools_submodules:
                    submodule_dict[sub.path] = sub

    # Remove unwanted upstream submodules
    for path in submodule_blacklist:
        if path in submodule_dict:
            del submodule_dict[path]

    return submodule_dict.values()

def findSnapshotBaselineSha1():
    if not os.path.isdir(snapshot_src_dir):
        return ''
    oldCwd = os.getcwd()
    os.chdir(snapshot_src_dir)
    line = subprocess.check_output(['git', 'log', '-n1', '--pretty=oneline', '--grep=' + currentVersion()])
    os.chdir(oldCwd)
    return line.split(' ')[0]

def preparePatchesFromSnapshot():
    oldCwd = os.getcwd()
    base_sha1 = findSnapshotBaselineSha1()
    if not base_sha1:
        sys.exit('-- base sha1 not found in ' + os.getcwd() + ' --')

    patches_dir = os.path.join(upstream_src_dir, 'patches')
    if os.path.isdir(patches_dir):
        shutil.rmtree(patches_dir)
    os.mkdir(patches_dir)

    os.chdir(snapshot_src_dir)
    print('-- preparing patches to ' + patches_dir + ' --')
    subprocess.call(['git', 'format-patch', '-q', '-o', patches_dir, base_sha1])

    os.chdir(patches_dir)
    patches = glob.glob('00*.patch')

    # We'll collect the patches for submodules in corresponding lists
    patches_dict = {}
    for patch in patches:
        patch_path = os.path.abspath(patch)
        with open(patch, 'r') as pfile:
            for line in pfile:
                if 'Subject:' in line:
                    match = re.search('<(.+)>', line)
                    if match:
                        submodule = match.group(1)
                        if submodule not in patches_dict:
                            patches_dict[submodule] = []
                        patches_dict[submodule].append(patch_path)

    os.chdir(oldCwd)
    return patches_dict

def resetUpstream():
    oldCwd = os.getcwd()
    target_dir = os.path.join(upstream_src_dir, 'chromium')
    os.chdir(target_dir)

    chromium = GitSubmodule.Submodule()
    chromium.path = "."
    submodules = chromium.readSubmodules()
    submodules.append(chromium)

    print('-- resetting upstream submodules in ' + os.path.relpath(target_dir) + ' to baseline --')

    for module in submodules:
        oldCwd = os.getcwd()
        module_path = os.path.abspath(os.path.join(target_dir, module.path))
        if not os.path.isdir(module_path):
            continue

        cwd = os.getcwd()
        os.chdir(module_path)
        line = subprocess.check_output(['git', 'log', '-n1', '--pretty=oneline', '--grep=-- QtWebEngine baseline --'])
        line = line.split(' ')[0]
        if line:
            subprocess.call(['git', 'reset', '-q', '--hard', line])
        os.chdir(cwd)
        module.reset()
    os.chdir(oldCwd)
    print('done.\n')
