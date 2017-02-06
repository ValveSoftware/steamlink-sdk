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
import imp
import errno
import shutil

from distutils.version import StrictVersion
import git_submodule as GitSubmodule

qtwebengine_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
os.chdir(qtwebengine_root)

def isInGitBlacklist(file_path):
    # We do need all the gyp files.
    if ( '.gitignore' in file_path
        or '.gitmodules' in file_path
        or '.gitattributes' in file_path
        or '.DEPS' in file_path ):
        return True

def isInChromiumBlacklist(file_path):
    # Filter out empty submodule directories.
    if (os.path.isdir(file_path)):
        return True
    # We do need all the gyp files.
    if file_path.endswith('.gyp') or file_path.endswith('.gypi') or file_path.endswith('.isolate'):
        return False
    # We do need all the gn file.
    if file_path.endswith('.gn') or file_path.endswith('.gni') or file_path.endswith('typemap') or \
    file_path.endswith('.mojom'):
        return False
    if ( '_jni' in file_path
        or 'jni_' in file_path
        or 'testdata/' in file_path
        or file_path.startswith('third_party/android_tools')
        or '/tests/' in file_path
        or ('/test/' in file_path and
            not '/webrtc/' in file_path and
            not file_path.startswith('net/test/') and
            not file_path.endswith('mock_chrome_application_mac.h') and
            not file_path.endswith('perftimer.h') and
            not 'ozone' in file_path)
        or file_path.endswith('.java')
        or file_path.startswith('android_webview')
        or file_path.startswith('apps/')
        or file_path.startswith('ash/')
        or file_path.startswith('athena')
        or file_path.startswith('base/android/java')
        or file_path.startswith('breakpad')
        or file_path.startswith('build/android/')
        or file_path.startswith('buildtools/clang_format/script')
        or (file_path.startswith('chrome/') and
            not file_path.startswith('chrome/VERSION') and
            not file_path.startswith('chrome/browser/chrome_notification_types.h') and
            not '/app/theme/' in file_path and
            not '/app/resources/' in file_path and
            not '/browser/printing/' in file_path and
            not ('/browser/resources/' in file_path and not '/chromeos/' in file_path) and
            not '/renderer/resources/' in file_path and
            not 'repack_locales' in file_path and
            not 'third_party/chromevox' in file_path and
            not 'media/desktop_media_list.h' in file_path and
            not 'media/desktop_streams_registry.' in file_path and
            not 'common/chrome_constants.' in file_path and
            not 'common/chrome_paths' in file_path and
            not 'common/chrome_switches.' in file_path and
            not 'common/content_restriction.h' in file_path and
            not 'common/spellcheck_' in file_path and
            not 'common/url_constants' in file_path and
            not '/extensions/api/' in file_path and
            not '/extensions/browser/api/' in file_path and
            not '/extensions/permissions/' in file_path and
            not '/renderer_host/pepper/' in file_path and
            not '/renderer/pepper/' in file_path and
            not '/spellchecker/' in file_path and
            not '/tools/convert_dict/' in file_path and
            not file_path.endswith('cf_resources.rc') and
            not file_path.endswith('version.py') and
            not file_path.endswith('.grd') and
            not file_path.endswith('.grdp') and
            not file_path.endswith('.json') and
            not file_path.endswith('chrome_version.rc.version'))
        or file_path.startswith('chrome_frame')
        or file_path.startswith('chromeos')
        or file_path.startswith('cloud_print')
        or file_path.startswith('components/chrome_apps/')
        or file_path.startswith('components/cronet/')
        or file_path.startswith('components/drive/')
        or file_path.startswith('components/invalidation/')
        or file_path.startswith('components/gcm_driver/')
        or file_path.startswith('components/mus/')
        or file_path.startswith('components/nacl/')
        or file_path.startswith('components/omnibox/')
        or file_path.startswith('components/policy/')
        or file_path.startswith('components/proximity_auth/')
        or (file_path.startswith('components/resources/terms/') and not file_path.endswith('terms_chromium.html'))
        or file_path.startswith('components/rlz/')
        or file_path.startswith('components/sync_driver/')
        or file_path.startswith('components/test/')
        or file_path.startswith('components/test_runner/')
        or file_path.startswith('components/translate/')
        or file_path.startswith('content/public/android/java')
        or (file_path.startswith('content/shell') and
            not file_path.startswith('content/shell/common') and
            not file_path.endswith('.grd'))
        or file_path.startswith('courgette')
        or file_path.startswith('google_update')
        or file_path.startswith('ios')
        or file_path.startswith('media/base/android/java')
        or file_path.startswith('native_client')
        or file_path.startswith('net/android/java')
        or file_path.startswith('remoting')
        or file_path.startswith('rlz')
        or file_path.startswith('sync')
        or file_path.startswith('testing/android')
        or file_path.startswith('testing/buildbot')
        or file_path.startswith('third_party/WebKit/LayoutTests')
        or file_path.startswith('third_party/WebKit/ManualTests')
        or file_path.startswith('third_party/WebKit/PerformanceTests')
        or file_path.startswith('third_party/accessibility-audit')
        or file_path.startswith('third_party/android_')
        or file_path.startswith('third_party/apache-win32')
        or file_path.startswith('third_party/apple_sample_code')
        or file_path.startswith('third_party/ashmem')
        or file_path.startswith('third_party/binutils')
        or file_path.startswith('third_party/bison')
        or (file_path.startswith('third_party/cacheinvalidation') and
            not file_path.endswith('isolate'))
        or file_path.startswith('third_party/boringssl/src/fuzz')
        or file_path.startswith('third_party/catapult')
        or file_path.startswith('third_party/chromite')
        or file_path.startswith('third_party/cld_2')
        or file_path.startswith('third_party/codesighs')
        or file_path.startswith('third_party/colorama')
        or file_path.startswith('third_party/cygwin')
        or file_path.startswith('third_party/cython')
        or file_path.startswith('third_party/deqp')
        or file_path.startswith('third_party/elfutils')
        or file_path.startswith('third_party/google_input_tools')
        or file_path.startswith('third_party/gperf')
        or file_path.startswith('third_party/gnu_binutils')
        or file_path.startswith('third_party/gtk+')
        or file_path.startswith('third_party/google_appengine_cloudstorage')
        or file_path.startswith('third_party/google_toolbox_for_mac')
        or file_path.startswith('third_party/hunspell_dictionaries')
        or (file_path.startswith('third_party/icu') and file_path.endswith('icudtl_dat.S'))
        or file_path.startswith('third_party/instrumented_libraries')
        or file_path.startswith('third_party/jsr-305/src')
        or file_path.startswith('third_party/junit')
        or file_path.startswith('third_party/lcov')
        or file_path.startswith('third_party/libphonenumber')
        or file_path.startswith('third_party/libaddressinput/src/testdata')
        or file_path.startswith('third_party/libaddressinput/src/common/src/test')
        or file_path.startswith('third_party/libc++')
        or file_path.startswith('third_party/liblouis')
        or file_path.startswith('third_party/lighttpd')
        or file_path.startswith('third_party/logilab')
        or file_path.startswith('third_party/markdown')
        or file_path.startswith('third_party/mingw-w64')
        or file_path.startswith('third_party/nacl_sdk_binaries')
        or (file_path.startswith('third_party/polymer') and
            not file_path.startswith('third_party/polymer/v1_0/components-chromium/'))
        or file_path.startswith('third_party/openh264/src/res')
        or file_path.startswith('third_party/pdfium/tools')
        or file_path.startswith('third_party/pdfsqueeze')
        or file_path.startswith('third_party/pefile')
        or file_path.startswith('third_party/perl')
        or file_path.startswith('third_party/psyco_win32')
        or file_path.startswith('third_party/pylint')
        or file_path.startswith('third_party/scons-2.0.1')
        or file_path.startswith('third_party/sfntly/src/cpp/data/fonts')
        or file_path.startswith('third_party/speech-dispatcher')
        or file_path.startswith('third_party/talloc')
        or file_path.startswith('third_party/trace-viewer')
        or file_path.startswith('third_party/undoview')
        or file_path.startswith('third_party/webgl')
        or file_path.startswith('tools/android')
        or file_path.startswith('tools/luci_go')
        or file_path.startswith('tools/metrics')
        or file_path.startswith('tools/memory_inspector')
        or file_path.startswith('tools/perf')
        or file_path.startswith('tools/swarming_client')
        or file_path.startswith('ui/android/java')
        or file_path.startswith('ui/app_list')
        or file_path.startswith('ui/base/ime/chromeos')
        or file_path.startswith('ui/chromeos')
        or file_path.startswith('ui/display/chromeos')
        or file_path.startswith('ui/events/ozone/chromeos')
        or file_path.startswith('ui/file_manager')
        or file_path.startswith('ui/gfx/chromeos')

        ):
            return True
    return False

def printProgress(current, total):
    sys.stdout.write("\r{} of {}".format(current, total))
    sys.stdout.flush()

def copyFile(src, dst):
    src = os.path.abspath(src)
    dst = os.path.abspath(dst)
    dst_dir = os.path.dirname(dst)

    if not os.path.isdir(dst_dir):
        os.makedirs(dst_dir)

    if os.path.exists(dst):
        os.remove(dst)

    try:
        os.link(src, dst)
        # Qt uses LF-only but Chromium isn't.
        subprocess.call(['dos2unix', '--keep-bom', '--quiet', dst])
    except OSError as exception:
        if exception.errno == errno.ENOENT:
            print 'file does not exist:' + src
        else:
            raise

third_party_upstream = os.path.join(qtwebengine_root, 'src/3rdparty_upstream')
third_party = os.path.join(qtwebengine_root, 'src/3rdparty')

def clearDirectory(directory):
    currentDir = os.getcwd()
    os.chdir(directory)
    print 'clearing the directory:' + directory
    for direntry in os.listdir(directory):
        if not direntry == '.git' and os.path.isdir(direntry):
            print 'clearing:' + direntry
            shutil.rmtree(direntry)
    os.chdir(currentDir)

def listFilesInCurrentRepository():
    currentRepo = GitSubmodule.Submodule(os.getcwd())
    files = subprocess.check_output(['git', 'ls-files']).splitlines()
    submodules = currentRepo.readSubmodules()
    for submodule in submodules:
        submodule_files = submodule.listFiles()
        for submodule_file in submodule_files:
            files.append(os.path.join(submodule.pathRelativeToTopMostSupermodule(), submodule_file))
    return files

def exportNinja():
    third_party_upstream_ninja = os.path.join(third_party_upstream, 'ninja')
    third_party_ninja = os.path.join(third_party, 'ninja')
    os.makedirs(third_party_ninja);
    print 'exporting contents of:' + third_party_upstream_ninja
    os.chdir(third_party_upstream_ninja)
    files = listFilesInCurrentRepository()
    print 'copying files to ' + third_party_ninja
    for i in xrange(len(files)):
        printProgress(i+1, len(files))
        f = files[i]
        if not isInGitBlacklist(f):
            copyFile(f, os.path.join(third_party_ninja, f))
    print("")

def exportChromium():
    third_party_upstream_chromium = os.path.join(third_party_upstream, 'chromium')
    third_party_chromium = os.path.join(third_party, 'chromium')
    os.makedirs(third_party_chromium);
    print 'exporting contents of:' + third_party_upstream_chromium
    os.chdir(third_party_upstream_chromium)
    files = listFilesInCurrentRepository()
    # Add LASTCHANGE files which are not tracked by git.
    files.append('build/util/LASTCHANGE')
    files.append('build/util/LASTCHANGE.blink')
    print 'copying files to ' + third_party_chromium
    for i in xrange(len(files)):
        printProgress(i+1, len(files))
        f = files[i]
        if not isInChromiumBlacklist(f) and not isInGitBlacklist(f):
            copyFile(f, os.path.join(third_party_chromium, f))
    print("")

commandNotFound = subprocess.call(['which', 'dos2unix'])

if not commandNotFound:
    dos2unixVersion , err = subprocess.Popen(['dos2unix', '-V', '| true'], stdout=subprocess.PIPE, stderr=subprocess.PIPE).communicate()
    if not dos2unixVersion:
        raise Exception("You need dos2unix version 6.0.6 minimum.")
    dos2unixVersion = StrictVersion(dos2unixVersion.splitlines()[0].split()[1])

if commandNotFound or dos2unixVersion < StrictVersion('6.0.6'):
    raise Exception("You need dos2unix version 6.0.6 minimum.")

os.chdir(third_party)
ignore_case_setting = subprocess.Popen(['git', 'config', '--get', 'core.ignorecase'], stdout=subprocess.PIPE).communicate()[0]
if 'true' in ignore_case_setting:
    raise Exception("Your 3rdparty repository is configured to ignore case. "
                    "A snapshot created with these settings would cause problems on case sensitive file systems.")

clearDirectory(third_party)

exportNinja()
exportChromium()

print 'done.'

