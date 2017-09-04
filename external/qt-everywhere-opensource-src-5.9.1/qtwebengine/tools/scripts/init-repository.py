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
import subprocess
import sys
import string
import argparse

qtwebengine_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))

import git_submodule as GitSubmodule
import version_resolver as resolver

chromium_src = os.environ.get('CHROMIUM_SRC_DIR')
ninja_src = os.path.join(qtwebengine_root, 'src/3rdparty_upstream/ninja')
use_external_chromium = False

parser = argparse.ArgumentParser(description='Initialize QtWebEngine repository.')
parser.add_argument('--baseline-upstream', action='store_true', help='initialize using upstream Chromium submodule w/o applying patches (for maintenance purposes only)')
group = parser.add_mutually_exclusive_group()
group.add_argument('-u', '--upstream', action='store_true', help='initialize using upstream Chromium submodule')
group.add_argument('-s', '--snapshot', action='store_true', help='initialize using flat Chromium snapshot submodule (default)')
args = parser.parse_args()

if args.baseline_upstream:
    args.upstream = True

if chromium_src:
    chromium_src = os.path.abspath(chromium_src)
    use_external_chromium = True
if not chromium_src or not os.path.isdir(chromium_src):
    if args.upstream:
        chromium_src = os.path.join(qtwebengine_root, 'src/3rdparty_upstream/chromium')
    if args.snapshot or not chromium_src:
        chromium_src = os.path.join(qtwebengine_root, 'src/3rdparty/chromium')
        ninja_src = os.path.join(qtwebengine_root, 'src/3rdparty/ninja')
        args.snapshot = True
    print 'CHROMIUM_SRC_DIR not set, using Chromium in' + chromium_src

if not args.baseline_upstream:
    # Write our chromium sources directory into git config.
    relative_chromium_src = os.path.relpath(chromium_src, qtwebengine_root)
    subprocess.call(['git', 'config', 'qtwebengine.chromiumsrcdir', relative_chromium_src])


def updateLastChange():
    if use_external_chromium:
        return
    currentDir = os.getcwd()
    os.chdir(chromium_src)
    print 'updating LASTCHANGE files'
    subprocess.call(['python', 'build/util/lastchange.py', '-o', 'build/util/LASTCHANGE'])
    subprocess.call(['python', 'build/util/lastchange.py', '-s', 'third_party/WebKit', '-o', 'build/util/LASTCHANGE.blink'])
    subprocess.call(['python', 'build/util/lastchange.py', '-m', 'SKIA_COMMIT_HASH', '-s', 'third_party/skia', '--header', 'skia/ext/skia_commit_hash.h'])
    os.chdir(currentDir)

def initUpstreamSubmodules():
    ninja_url = 'https://github.com/martine/ninja.git'
    chromium_url = 'https://chromium.googlesource.com/chromium/src.git'
    ninja_shasum = 'refs/tags/' + resolver.currentNinjaVersion()
    chromium_ref = 'refs/tags/' + resolver.currentVersion()
    os.chdir(qtwebengine_root)

    current_submodules = subprocess.check_output(['git', 'submodule'])
    if not 'src/3rdparty_upstream/ninja' in current_submodules:
        subprocess.call(['git', 'submodule', 'add', ninja_url, 'src/3rdparty_upstream/ninja'])
    if not use_external_chromium and not 'src/3rdparty_upstream/chromium' in current_submodules:
        subprocess.call(['git', 'submodule', 'add', chromium_url, 'src/3rdparty_upstream/chromium'])

    ninjaSubmodule = GitSubmodule.Submodule()
    ninjaSubmodule.path = 'src/3rdparty_upstream/ninja'
    ninjaSubmodule.ref = ninja_shasum
    ninjaSubmodule.url = ninja_url
    ninjaSubmodule.os = 'all'
    ninjaSubmodule.initialize()

    if not use_external_chromium:
        chromiumSubmodule = GitSubmodule.Submodule()
        chromiumSubmodule.path = 'src/3rdparty_upstream/chromium'
        chromiumSubmodule.ref = chromium_ref
        chromiumSubmodule.url = chromium_url
        chromiumSubmodule.os = 'all'
        chromiumSubmodule.initialize()
        chromiumSubmodule.initSubmodules()

    # Unstage repositories so we do not accidentally commit them.
    subprocess.call(['git', 'reset', '-q', 'HEAD', 'src/3rdparty_upstream/ninja'])
    subprocess.call(['git', 'reset', '-q', 'HEAD', 'src/3rdparty_upstream/chromium'])

def initSnapshot():
    snapshot = GitSubmodule.Submodule()
    snapshot.path = 'src/3rdparty'
    snapshot.os = 'all'
    snapshot.initialize()

os.chdir(qtwebengine_root)

if args.upstream:
    initUpstreamSubmodules()
    updateLastChange()
    if not args.baseline_upstream and not use_external_chromium:
        subprocess.call(['python', os.path.join(qtwebengine_root, 'tools', 'scripts', 'patch_upstream.py')])
if args.snapshot:
    initSnapshot()
