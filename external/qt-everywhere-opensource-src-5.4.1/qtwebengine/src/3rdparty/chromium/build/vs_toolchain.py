# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import pipes
import shutil
import subprocess
import sys


script_dir = os.path.dirname(os.path.realpath(__file__))
chrome_src = os.path.abspath(os.path.join(script_dir, os.pardir))
SRC_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(1, os.path.join(chrome_src, 'tools'))
sys.path.insert(0, os.path.join(chrome_src, 'tools', 'gyp', 'pylib'))
json_data_file = os.path.join(script_dir, 'win_toolchain.json')


import gyp


def SetEnvironmentAndGetRuntimeDllDirs():
  """Sets up os.environ to use the depot_tools VS toolchain with gyp, and
  returns the location of the VS runtime DLLs so they can be copied into
  the output directory after gyp generation.
  """
  vs2013_runtime_dll_dirs = None
  depot_tools_win_toolchain = \
      bool(int(os.environ.get('DEPOT_TOOLS_WIN_TOOLCHAIN', '1')))
  if sys.platform in ('win32', 'cygwin') and depot_tools_win_toolchain:
    with open(json_data_file, 'r') as tempf:
      toolchain_data = json.load(tempf)

    toolchain = toolchain_data['path']
    version = toolchain_data['version']
    version_is_pro = version[-1] != 'e'
    win8sdk = toolchain_data['win8sdk']
    wdk = toolchain_data['wdk']
    # TODO(scottmg): The order unfortunately matters in these. They should be
    # split into separate keys for x86 and x64. (See CopyVsRuntimeDlls call
    # below). http://crbug.com/345992
    vs2013_runtime_dll_dirs = toolchain_data['runtime_dirs']

    os.environ['GYP_MSVS_OVERRIDE_PATH'] = toolchain
    os.environ['GYP_MSVS_VERSION'] = version
    # We need to make sure windows_sdk_path is set to the automated
    # toolchain values in GYP_DEFINES, but don't want to override any
    # otheroptions.express
    # values there.
    gyp_defines_dict = gyp.NameValueListToDict(gyp.ShlexEnv('GYP_DEFINES'))
    gyp_defines_dict['windows_sdk_path'] = win8sdk
    os.environ['GYP_DEFINES'] = ' '.join('%s=%s' % (k, pipes.quote(str(v)))
        for k, v in gyp_defines_dict.iteritems())
    os.environ['WINDOWSSDKDIR'] = win8sdk
    os.environ['WDK_DIR'] = wdk
    # Include the VS runtime in the PATH in case it's not machine-installed.
    runtime_path = ';'.join(vs2013_runtime_dll_dirs)
    os.environ['PATH'] = runtime_path + ';' + os.environ['PATH']
  return vs2013_runtime_dll_dirs


def CopyVsRuntimeDlls(output_dir, runtime_dirs):
  """Copies the VS runtime DLLs from the given |runtime_dirs| to the output
  directory so that even if not system-installed, built binaries are likely to
  be able to run.

  This needs to be run after gyp has been run so that the expected target
  output directories are already created.
  """
  assert sys.platform.startswith(('win32', 'cygwin'))

  def copy_runtime(target_dir, source_dir, dll_pattern):
    """Copy both the msvcr and msvcp runtime DLLs, only if the target doesn't
    exist, but the target directory does exist."""
    for which in ('p', 'r'):
      dll = dll_pattern % which
      target = os.path.join(target_dir, dll)
      source = os.path.join(source_dir, dll)
      # If gyp generated to that output dir, and the runtime isn't already
      # there, then copy it over.
      if (os.path.isdir(target_dir) and
          (not os.path.isfile(target) or
            os.stat(target).st_mtime != os.stat(source).st_mtime)):
        print 'Copying %s to %s...' % (source, target)
        if os.path.exists(target):
          os.unlink(target)
        shutil.copy2(source, target)

  x86, x64 = runtime_dirs
  out_debug = os.path.join(output_dir, 'Debug')
  out_debug_nacl64 = os.path.join(output_dir, 'Debug', 'x64')
  out_release = os.path.join(output_dir, 'Release')
  out_release_nacl64 = os.path.join(output_dir, 'Release', 'x64')
  out_debug_x64 = os.path.join(output_dir, 'Debug_x64')
  out_release_x64 = os.path.join(output_dir, 'Release_x64')

  if os.path.exists(out_debug) and not os.path.exists(out_debug_nacl64):
    os.makedirs(out_debug_nacl64)
  if os.path.exists(out_release) and not os.path.exists(out_release_nacl64):
    os.makedirs(out_release_nacl64)
  copy_runtime(out_debug,          x86, 'msvc%s120d.dll')
  copy_runtime(out_release,        x86, 'msvc%s120.dll')
  copy_runtime(out_debug_x64,      x64, 'msvc%s120d.dll')
  copy_runtime(out_release_x64,    x64, 'msvc%s120.dll')
  copy_runtime(out_debug_nacl64,   x64, 'msvc%s120d.dll')
  copy_runtime(out_release_nacl64, x64, 'msvc%s120.dll')


def _GetDesiredVsToolchainHashes():
  """Load a list of SHA1s corresponding to the toolchains that we want installed
  to build with."""
  sha1path = os.path.join(script_dir, 'toolchain_vs2013.hash')
  with open(sha1path, 'rb') as f:
    return f.read().strip().splitlines()


def Update():
  """Requests an update of the toolchain to the specific hashes we have at
  this revision. The update outputs a .json of the various configuration
  information required to pass to gyp which we use in |GetToolchainDir()|.
  """
  depot_tools_win_toolchain = \
      bool(int(os.environ.get('DEPOT_TOOLS_WIN_TOOLCHAIN', '1')))
  if sys.platform in ('win32', 'cygwin') and depot_tools_win_toolchain:
    import find_depot_tools
    depot_tools_path = find_depot_tools.add_depot_tools_to_path()
    json_data_file = os.path.join(script_dir, 'win_toolchain.json')
    get_toolchain_args = [
        sys.executable,
        os.path.join(depot_tools_path,
                    'win_toolchain',
                    'get_toolchain_if_necessary.py'),
        '--output-json', json_data_file,
      ] + _GetDesiredVsToolchainHashes()
    subprocess.check_call(get_toolchain_args)

  return 0


def GetToolchainDir():
  """Gets location information about the current toolchain (must have been
  previously updated by 'update'). This is used for the GN build."""
  SetEnvironmentAndGetRuntimeDllDirs()
  print '''vs_path = "%s"
sdk_path = "%s"
vs_version = "%s"
wdk_dir = "%s"
''' % (
      os.environ['GYP_MSVS_OVERRIDE_PATH'],
      os.environ['WINDOWSSDKDIR'],
      os.environ['GYP_MSVS_VERSION'],
      os.environ['WDK_DIR'])


def main():
  if not sys.platform.startswith(('win32', 'cygwin')):
    return 0
  commands = {
      'update': Update,
      'get_toolchain_dir': GetToolchainDir,
      # TODO(scottmg): Add copy_dlls for GN builds (gyp_chromium calls
      # CopyVsRuntimeDlls via import, currently).
  }
  if len(sys.argv) < 2 or sys.argv[1] not in commands:
    print >>sys.stderr, 'Expected one of: %s' % ', '.join(commands)
    return 1
  return commands[sys.argv[1]]()


if __name__ == '__main__':
  sys.exit(main())
