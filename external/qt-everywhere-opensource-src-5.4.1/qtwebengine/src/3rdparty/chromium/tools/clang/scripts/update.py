#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Windows can't run .sh files, so this is a Python implementation of
update.sh. This script should replace update.sh on all platforms eventually."""

import os
import re
import shutil
import subprocess
import sys

# Do NOT CHANGE this if you don't know what you're doing -- see
# https://code.google.com/p/chromium/wiki/UpdatingClang
# Reverting problematic clang rolls is safe, though.
# Note: this revision is only used for Windows. Other platforms use update.sh.
LLVM_WIN_REVISION = 'HEAD'

# ASan on Windows is useful enough to use it even while the clang/win is still
# in bringup. Use a pinned revision to make it slightly more stable.
if (re.search(r'\b(asan)=1', os.environ.get('GYP_DEFINES', '')) and
    not 'LLVM_FORCE_HEAD_REVISION' in os.environ):
  LLVM_WIN_REVISION = '210586'

# Path constants. (All of these should be absolute paths.)
THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..'))
LLVM_DIR = os.path.join(CHROMIUM_DIR, 'third_party', 'llvm')
LLVM_BUILD_DIR = os.path.join(CHROMIUM_DIR, 'third_party', 'llvm-build',
                              'Release+Asserts')
COMPILER_RT_BUILD_DIR = os.path.join(LLVM_BUILD_DIR, '32bit-compiler-rt')
CLANG_DIR = os.path.join(LLVM_DIR, 'tools', 'clang')
COMPILER_RT_DIR = os.path.join(LLVM_DIR, 'projects', 'compiler-rt')
STAMP_FILE = os.path.join(LLVM_BUILD_DIR, 'cr_build_revision')

LLVM_REPO_URL='https://llvm.org/svn/llvm-project'
if 'LLVM_REPO_URL' in os.environ:
  LLVM_REPO_URL = os.environ['LLVM_REPO_URL']


def ReadStampFile():
  """Return the contents of the stamp file, or '' if it doesn't exist."""
  try:
    with open(STAMP_FILE, 'r') as f:
      return f.read();
  except IOError:
    return ''


def WriteStampFile(s):
  """Write s to the stamp file."""
  if not os.path.exists(LLVM_BUILD_DIR):
    os.makedirs(LLVM_BUILD_DIR)
  with open(STAMP_FILE, 'w') as f:
    f.write(s)


def DeleteFiles(dir, pattern):
  """Delete all files in dir matching pattern."""
  n = 0
  regex = re.compile(r'^' + pattern + r'$')
  for root, _, files in os.walk(dir):
    for f in files:
      if regex.match(f):
        os.remove(os.path.join(root, f))
        n += 1
  return n


def ClobberChromiumBuildFiles():
  """Clobber Chomium build files."""
  print 'Clobbering Chromium build files...'
  out_dir = os.path.join(CHROMIUM_DIR, 'out')
  if os.path.isdir(out_dir):
    shutil.rmtree(out_dir)
    print 'Removed Chromium out dir: %s.' % (out_dir)


def RunCommand(command, tries=1):
  """Run a command, possibly with multiple retries."""
  for i in range(0, tries):
    print 'Running %s (try #%d)' % (str(command), i + 1)
    if subprocess.call(command, shell=True) == 0:
      return
    print 'Failed.'
  sys.exit(1)

def CopyFile(src, dst):
  """Copy a file from src to dst."""
  shutil.copy(src, dst)
  print "Copying %s to %s" % (src, dst)

def Checkout(name, url, dir):
  """Checkout the SVN module at url into dir. Use name for the log message."""
  print "Checking out %s r%s into '%s'" % (name, LLVM_WIN_REVISION, dir)
  RunCommand(['svn', 'checkout', '--force',
              url + '@' + LLVM_WIN_REVISION, dir], tries=2)


vs_version = None
def GetVSVersion():
  global vs_version
  if not vs_version:
    # TODO(hans): Find a less hacky way to find the MSVS installation.
    sys.path.append(os.path.join(CHROMIUM_DIR, 'tools', 'gyp', 'pylib'))
    import gyp.MSVSVersion
    # We request VS 2013 because Clang won't build with 2010, and 2013 will be
    # the default for Chromium soon anyway.
    vs_version = gyp.MSVSVersion.SelectVisualStudioVersion('2013')
  return vs_version


def UpdateClang():
  print 'Updating Clang to %s...' % (LLVM_WIN_REVISION)
  if LLVM_WIN_REVISION != 'HEAD' and ReadStampFile() == LLVM_WIN_REVISION:
    print 'Already up to date.'
    return 0

  ClobberChromiumBuildFiles()

  # Reset the stamp file in case the build is unsuccessful.
  WriteStampFile('')

  Checkout('LLVM', LLVM_REPO_URL + '/llvm/trunk', LLVM_DIR)
  Checkout('Clang', LLVM_REPO_URL + '/cfe/trunk', CLANG_DIR)
  Checkout('compiler-rt', LLVM_REPO_URL + '/compiler-rt/trunk', COMPILER_RT_DIR)

  if not os.path.exists(LLVM_BUILD_DIR):
    os.makedirs(LLVM_BUILD_DIR)
  os.chdir(LLVM_BUILD_DIR)

  if not re.search(r'cmake', os.environ['PATH'], flags=re.IGNORECASE):
    # If CMake is not on the path, try looking in a standard location.
    os.environ['PATH'] += os.pathsep + 'C:\\Program Files (x86)\\CMake 2.8\\bin'

  RunCommand(GetVSVersion().SetupScript('x64') +
             ['&&', 'cmake', '-GNinja', '-DCMAKE_BUILD_TYPE=Release',
              '-DLLVM_ENABLE_ASSERTIONS=ON', LLVM_DIR])
  RunCommand(GetVSVersion().SetupScript('x64') + ['&&', 'ninja', 'all'])

  # Do an x86 build of compiler-rt to get the 32-bit ASan run-time.
  # TODO(hans): Remove once the regular build above produces this.
  if not os.path.exists(COMPILER_RT_BUILD_DIR):
    os.makedirs(COMPILER_RT_BUILD_DIR)
  os.chdir(COMPILER_RT_BUILD_DIR)
  RunCommand(GetVSVersion().SetupScript('x86') +
             ['&&', 'cmake', '-GNinja', '-DCMAKE_BUILD_TYPE=Release',
              '-DLLVM_ENABLE_ASSERTIONS=ON', LLVM_DIR])
  RunCommand(GetVSVersion().SetupScript('x86') + ['&&', 'ninja', 'compiler-rt'])

  # TODO(hans): Make this (and the .gypi file) version number independent.
  asan_rt_lib_src_dir = os.path.join(COMPILER_RT_BUILD_DIR, 'lib', 'clang',
                                     '3.5.0', 'lib', 'windows')
  asan_rt_lib_dst_dir = os.path.join(LLVM_BUILD_DIR, 'lib', 'clang',
                                     '3.5.0', 'lib', 'windows')

  if not os.path.exists(asan_rt_lib_dst_dir):
    os.makedirs(asan_rt_lib_dst_dir)
  for root, _, files in os.walk(asan_rt_lib_src_dir):
    for f in files:
      if re.match(r'^.*-i386\.lib$', f):
        CopyFile(os.path.join(root, f), asan_rt_lib_dst_dir)

  CopyFile(os.path.join(asan_rt_lib_src_dir, '..', '..', 'asan_blacklist.txt'),
           os.path.join(asan_rt_lib_dst_dir, '..', '..'))

  # Make an extra copy of the sanitizer headers, to be put on the include path
  # of the fallback compiler.
  sanitizer_include_dir = os.path.join(LLVM_BUILD_DIR, 'lib', 'clang', '3.5.0',
                                       'include', 'sanitizer')
  aux_sanitizer_include_dir = os.path.join(LLVM_BUILD_DIR, 'lib', 'clang',
                                           '3.5.0', 'include_sanitizer',
                                           'sanitizer')
  if not os.path.exists(aux_sanitizer_include_dir):
    os.makedirs(aux_sanitizer_include_dir)
  for _, _, files in os.walk(sanitizer_include_dir):
    for f in files:
      CopyFile(os.path.join(sanitizer_include_dir, f),
               aux_sanitizer_include_dir)

  WriteStampFile(LLVM_WIN_REVISION)
  print 'Clang update was successful.'
  return 0


def main():
  if not sys.platform in ['win32', 'cygwin']:
    # For non-Windows, fall back to update.sh.
    # TODO(hans): Make update.py replace update.sh completely.

    # This script is called by gclient. gclient opens its hooks subprocesses
    # with (stdout=subprocess.PIPE, stderr=subprocess.STDOUT) and then does
    # custom output processing that breaks printing '\r' characters for
    # single-line updating status messages as printed by curl and wget.
    # Work around this by setting stderr of the update.sh process to stdin (!):
    # gclient doesn't redirect stdin, and while stdin itself is read-only, a
    # dup()ed sys.stdin is writable, try
    #   fd2 = os.dup(sys.stdin.fileno()); os.write(fd2, 'hi')
    # TODO: Fix gclient instead, http://crbug.com/95350
    return subprocess.call(
        [os.path.join(os.path.dirname(__file__), 'update.sh')] +  sys.argv[1:],
        stderr=os.fdopen(os.dup(sys.stdin.fileno())))

  if not re.search(r'\b(clang|asan)=1', os.environ.get('GYP_DEFINES', '')):
    print 'Skipping Clang update (clang=1 was not set in GYP_DEFINES).'
    return 0

  if re.search(r'\b(make_clang_dir)=', os.environ.get('GYP_DEFINES', '')):
    print 'Skipping Clang update (make_clang_dir= was set in GYP_DEFINES).'
    return 0

  return UpdateClang()


if __name__ == '__main__':
  sys.exit(main())
