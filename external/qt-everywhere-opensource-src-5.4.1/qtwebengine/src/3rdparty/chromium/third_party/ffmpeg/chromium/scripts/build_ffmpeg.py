#!/usr/bin/env python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import collections
import functools
import multiprocessing
import optparse
import os
import platform
import re
import shutil
import subprocess
import sys


SCRIPTS_DIR = os.path.abspath(os.path.dirname(__file__))
FFMPEG_DIR = os.path.abspath(os.path.join(SCRIPTS_DIR, '..', '..'))


USAGE = """Usage: %prog TARGET_OS TARGET_ARCH [options] -- [configure_args]

Valid combinations are linux       [ia32|x64|mipsel|arm|arm-neon]
                       linux-noasm [ia32|x64]
                       mac         [ia32|x64]
                       win         [ia32|x64]

Platform specific build notes:
  linux ia32/x64:
    Script can run on a normal Ubuntu box.

  linux mipsel:
    Script can run on a normal Ubuntu box with MIPS cross-toolchain in $PATH.

  linux arm/arm-neon:
    Script must be run inside of ChromeOS chroot with BOARD=arm-generic.

  mac:
    Script must be run on OSX.  Additionally, ensure the Chromium (not Apple)
    version of clang is in the path; usually found under
    src/third_party/llvm-build/Release+Asserts/bin

  win:
    Script must be run on Windows with VS2013 or higher under Cygwin (or MinGW,
    but as of 1.0.11, it has serious performance issues with make which makes
    building take hours).

    Additionall, ensure you have the correct toolchain environment for building.
    The x86 toolchain environment is required for ia32 builds and the x64 one
    for x64 builds.  This can be verified by running "cl.exe" and checking if
    the version string ends with "for x64" or "for x86."

    Building on Windows also requires some additional Cygwin packages plus a
    wrapper script for converting Cygwin paths to DOS paths.
      - Add these packages at install time: diffutils, yasm, make, python.
      - Copy chromium/scripts/cygwin-wrapper to /usr/local/bin

Resulting binaries will be placed in:
  build.TARGET_ARCH.TARGET_OS/Chromium/out/
  build.TARGET_ARCH.TARGET_OS/Chrome/out/
  build.TARGET_ARCH.TARGET_OS/ChromiumOS/out/
  build.TARGET_ARCH.TARGET_OS/ChromeOS/out/"""


def PrintAndCheckCall(argv, *args, **kwargs):
  print('Running %r' % argv)
  subprocess.check_call(argv, *args, **kwargs)


def DetermineHostOsAndArch():
  if platform.system() == 'Linux':
    host_os = 'linux'
  elif platform.system() == 'Darwin':
    host_os = 'mac'
  elif platform.system() == 'Windows' or platform.system() == 'CYGWIN_NT-6.1':
    host_os = 'win'
  else:
    return None

  if re.match(r'i.86', platform.machine()):
    host_arch = 'ia32'
  elif platform.machine() == 'x86_64' or platform.machine() == 'AMD64':
    host_arch = 'x64'
  elif platform.machine().startswith('arm'):
    host_arch = 'arm'
  else:
    return None

  return (host_os, host_arch)


def GetDsoName(target_os, dso_name, dso_version):
  if target_os == 'linux':
    return 'lib%s.so.%s' % (dso_name, dso_version)
  elif target_os == 'mac':
    return 'lib%s.%s.dylib' % (dso_name, dso_version)
  elif target_os == 'win':
    return '%s-%s.dll' % (dso_name, dso_version)
  else:
    raise ValueError('Unexpected target_os %s' % target_os)


def RewriteFile(path, search, replace):
  with open(path) as f:
    contents = f.read()
  with open(path, 'w') as f:
    f.write(re.sub(search, replace, contents))


def BuildFFmpeg(target_os, target_arch, host_os, host_arch, parallel_jobs,
                config_only, config, configure_flags):
  print('%s configure/build:' % config)

  config_dir = 'build.%s.%s/%s' % (target_arch, target_os, config)
  shutil.rmtree(config_dir, ignore_errors=True)
  os.makedirs(os.path.join(config_dir, 'out'))

  PrintAndCheckCall(
      [os.path.join(FFMPEG_DIR, 'configure')] + configure_flags, cwd=config_dir)

  if (target_os, target_arch) == ('mac', 'ia32'):
    # Required to get Mac ia32 builds compiling with -fno-omit-frame-pointer,
    # which is required for accurate stack traces.  See http://crbug.com/115170.
    #
    # Without this, building without -fomit-frame-pointer on ia32 will result in
    # the the inclusion of a number of inline assembly blocks that use too many
    # registers for its input/output operands.
    for name in ('config.h', 'config.asm'):
      RewriteFile(os.path.join(config_dir, name),
                  'HAVE_EBP_AVAILABLE 1',
                  'HAVE_EBP_AVAILABLE 0')

  if host_os == target_os and not config_only:
    libraries = [
        os.path.join('libavcodec', GetDsoName(target_os, 'avcodec', 55)),
        os.path.join('libavformat', GetDsoName(target_os, 'avformat', 55)),
        os.path.join('libavutil', GetDsoName(target_os, 'avutil', 52)),
    ]
    PrintAndCheckCall(
        ['make', '-j%d' % parallel_jobs] + libraries, cwd=config_dir)
    for lib in libraries:
      shutil.copy(os.path.join(config_dir, lib),
                  os.path.join(config_dir, 'out'))
  elif config_only:
    print('Skipping build step as requested.')
  else:
    print('Skipping compile as host configuration differs from target.\n'
          'Please compare the generated config.h with the previous version.\n'
          'You may also patch the script to properly cross-compile.\n'
          'Host OS : %s\n'
          'Target OS : %s\n'
          'Host arch : %s\n'
          'Target arch : %s\n' % (host_os, target_os, host_arch, target_arch))

  if target_arch in ('arm', 'arm-neon'):
    RewriteFile(
        os.path.join(config_dir, 'config.h'),
        r'(#define HAVE_VFP_ARGS [01])',
        r'/* \1 -- Disabled to allow softfp/hardfp selection at gyp time */')


def main(argv):
  parser = optparse.OptionParser(usage=USAGE)
  parser.add_option('--config-only', action='store_true',
                    help='Skip the build step. Useful when a given platform '
                    'is not necessary for generate_gyp.py')
  options, args = parser.parse_args(argv)

  if len(args) < 2:
    parser.print_help()
    return 1

  target_os = args[0]
  target_arch = args[1]
  configure_args = args[2:]

  if target_os not in ('linux', 'linux-noasm', 'win', 'win-vs2013', 'mac'):
    parser.print_help()
    return 1

  host_tuple = DetermineHostOsAndArch()
  if not host_tuple:
    print('Unrecognized host OS and architecture.', file=sys.stderr)
    return 1

  host_os, host_arch = host_tuple
  parallel_jobs = multiprocessing.cpu_count()

  print('System information:\n'
        'Host OS       : %s\n'
        'Target OS     : %s\n'
        'Host arch     : %s\n'
        'Target arch   : %s\n'
        'Parallel jobs : %d\n' % (
            host_os, target_os, host_arch, target_arch, parallel_jobs))

  configure_flags = collections.defaultdict(list)

  # Common configuration.  Note: --disable-everything does not in fact disable
  # everything, just non-library components such as decoders and demuxers.
  configure_flags['Common'].extend([
      '--disable-everything',
      '--disable-all',
      '--disable-doc',
      '--disable-static',
      '--enable-avcodec',
      '--enable-avformat',
      '--enable-avutil',
      '--enable-fft',
      '--enable-rdft',
      '--enable-shared',

      # Disable features.
      '--disable-bzlib',
      '--disable-error-resilience',
      '--disable-iconv',
      '--disable-lzo',
      '--disable-network',
      '--disable-symver',
      '--disable-xlib',
      '--disable-zlib',

      # Disable hardware decoding options which will sometimes turn on
      # via autodetect.
      '--disable-dxva2',
      '--disable-vaapi',
      '--disable-vda',
      '--disable-vdpau',

      # Common codecs.
      '--enable-decoder=theora,vorbis,vp8',
      '--enable-decoder=pcm_u8,pcm_s16le,pcm_s24le,pcm_f32le',
      '--enable-decoder=pcm_s16be,pcm_s24be,pcm_mulaw,pcm_alaw',
      '--enable-demuxer=ogg,matroska,wav',
      '--enable-parser=opus,vp3,vorbis,vp8',
  ])

  # --optflags doesn't append multiple entries, so set all at once.
  if (target_os, target_arch) == ('mac', 'ia32'):
    configure_flags['Common'].append('--optflags="-fno-omit-frame-pointer -O2"')
  else:
    configure_flags['Common'].append('--optflags="-O2"')

  # Linux only.
  if target_os in ('linux', 'linux-noasm'):
    if target_arch == 'x64':
      pass
    elif target_arch == 'ia32':
      configure_flags['Common'].extend([
          '--arch=i686',
          '--enable-yasm',
          '--extra-cflags="-m32"',
          '--extra-ldflags="-m32"',
      ])
    elif target_arch == 'arm':
      if host_arch != 'arm':
        configure_flags['Common'].extend([
            # This if-statement essentially is for chroot tegra2.
            '--enable-cross-compile',

            # Location is for CrOS chroot. If you want to use this, enter chroot
            # and copy ffmpeg to a location that is reachable.
            '--cross-prefix=/usr/bin/armv7a-cros-linux-gnueabi-',
            '--target-os=linux',
            '--arch=arm',
        ])

      # TODO(ihf): ARM compile flags are tricky. The final options
      # overriding everything live in chroot /build/*/etc/make.conf
      # (some of them coming from src/overlays/overlay-<BOARD>/make.conf).
      # We try to follow these here closely. In particular we need to
      # set ffmpeg internal #defines to conform to make.conf.
      # TODO(ihf): For now it is not clear if thumb or arm settings would be
      # faster. I ran experiments in other contexts and performance seemed
      # to be close and compiler version dependent. In practice thumb builds are
      # much smaller than optimized arm builds, hence we go with the global
      # CrOS settings.
      configure_flags['Common'].extend([
          '--enable-armv6',
          '--enable-armv6t2',
          '--enable-vfp',
          '--enable-thumb',
          '--disable-neon',
          '--extra-cflags=-march=armv7-a',
          '--extra-cflags=-mtune=cortex-a8',
          '--extra-cflags=-mfpu=vfpv3-d16',
          # NOTE: softfp/hardfp selected at gyp time.
          '--extra-cflags=-mfloat-abi=hard',
      ])
    elif target_arch == 'arm-neon':
      if host_arch != 'arm':
        # This if-statement is for chroot arm-generic.
        configure_flags['Common'].extend([
            '--enable-cross-compile',
            '--cross-prefix=/usr/bin/armv7a-cros-linux-gnueabi-',
            '--target-os=linux',
            '--arch=arm',
        ])
      configure_flags['Common'].extend([
          '--enable-armv6',
          '--enable-armv6t2',
          '--enable-vfp',
          '--enable-thumb',
          '--enable-neon',
          '--extra-cflags=-march=armv7-a',
          '--extra-cflags=-mtune=cortex-a8',
          '--extra-cflags=-mfpu=neon',
          # NOTE: softfp/hardfp selected at gyp time.
          '--extra-cflags=-mfloat-abi=hard',
      ])
    elif target_arch == 'mipsel':
      configure_flags['Common'].extend([
          '--enable-cross-compile',
          '--cross-prefix=mips-linux-gnu-',
          '--target-os=linux',
          '--arch=mips',
          '--extra-cflags=-mips32',
          '--extra-cflags=-EL',
          '--extra-ldflags=-mips32',
          '--extra-ldflags=-EL',
          '--disable-mipsfpu',
          '--disable-mipsdspr1',
          '--disable-mipsdspr2',
      ])
    else:
      print('Error: Unknown target arch %r for target OS %r!' % (
          target_arch, target_os), file=sys.stderr)
      return 1

  if target_os == 'linux-noasm':
    configure_flags['Common'].extend([
        '--disable-asm',
        '--disable-inline-asm',
    ])

  if 'win' not in target_os:
    configure_flags['Common'].append('--enable-pic')

  # Should be run on Mac.
  if target_os == 'mac':
    if host_os != 'mac':
      print('Script should be run on a Mac host. If this is not possible\n'
            'try a merge of config files with new linux ia32 config.h\n'
            'by hand.\n', file=sys.stderr)
      return 1

    configure_flags['Common'].extend([
        '--enable-yasm',
        '--cc=clang',
        '--cxx=clang++',
    ])
    if target_arch == 'ia32':
      configure_flags['Common'].extend([
          '--arch=i686',
          '--extra-cflags=-m32',
          '--extra-ldflags=-m32',
      ])
    elif target_arch == 'x64':
      configure_flags['Common'].extend([
          '--arch=x86_64',
          '--extra-cflags=-m64',
          '--extra-ldflags=-m64',
      ])
    else:
      print('Error: Unknown target arch %r for target OS %r!' % (
          target_arch, target_os), file=sys.stderr)

  # Should be run on Windows.
  if target_os == 'win':
    if host_os != 'win':
      print('Script should be run on a Windows host.\n', file=sys.stderr)
      return 1

    configure_flags['Common'].extend([
        '--toolchain=msvc',
        '--enable-yasm',
        '--extra-cflags=-I' + os.path.join(FFMPEG_DIR, 'chromium/include/win'),
    ])

    if platform.system() == 'CYGWIN_NT-6.1':
      configure_flags['Common'].extend([
          '--cc=cygwin-wrapper cl',
          '--ld=cygwin-wrapper link',
          '--nm=cygwin-wrapper dumpbin -symbols',
          '--ar=cygwin-wrapper lib',
      ])

  # Google Chrome & ChromeOS specific configuration.
  configure_flags['Chrome'].extend([
      '--enable-decoder=aac,h264,mp3',
      '--enable-demuxer=aac,mp3,mov',
      '--enable-parser=aac,h264,mpegaudio',
  ])

  # ChromiumOS specific configuration.
  # Warning: do *NOT* add avi, aac, h264, mp3, mp4, amr*
  # Flac support.
  configure_flags['ChromiumOS'].extend([
      '--enable-demuxer=flac',
      '--enable-decoder=flac',
      '--enable-parser=flac',
  ])

  # Google ChromeOS specific configuration.
  # We want to make sure to play everything Android generates and plays.
  # http://developer.android.com/guide/appendix/media-formats.html
  configure_flags['ChromeOS'].extend([
      # Enable playing avi files.
      '--enable-decoder=mpeg4',
      '--enable-parser=h263,mpeg4video',
      '--enable-demuxer=avi',
      # Enable playing Android 3gp files.
      '--enable-demuxer=amr',
      '--enable-decoder=amrnb,amrwb',
      # Flac support.
      '--enable-demuxer=flac',
      '--enable-decoder=flac',
      '--enable-parser=flac',
      # Wav files for playing phone messages.
      '--enable-decoder=gsm_ms',
      '--enable-demuxer=gsm',
      '--enable-parser=gsm',
  ])

  do_build_ffmpeg = functools.partial(
      BuildFFmpeg, target_os, target_arch, host_os, host_arch, parallel_jobs,
      options.config_only)
  do_build_ffmpeg('Chromium',
                  configure_flags['Common'] +
                  configure_flags['Chromium'] +
                  configure_args)
  do_build_ffmpeg('Chrome',
                  configure_flags['Common'] +
                  configure_flags['Chrome'] +
                  configure_args)

  if target_os == 'linux':
    do_build_ffmpeg('ChromiumOS',
                    configure_flags['Common'] +
                    configure_flags['Chromium'] +
                    configure_flags['ChromiumOS'] +
                    configure_args)

    # ChromeOS enables MPEG4 which requires error resilience :(
    chrome_os_flags = (configure_flags['Common'] +
                       configure_flags['Chrome'] +
                       configure_flags['ChromeOS'] +
                       configure_args)
    chrome_os_flags.remove('--disable-error-resilience')
    do_build_ffmpeg('ChromeOS', chrome_os_flags)

  print('Done. If desired you may copy config.h/config.asm into the '
        'source/config tree using copy_config.sh.')
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
