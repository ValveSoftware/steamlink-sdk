# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libusb',
      'type': 'static_library',
      'sources': [
        'src/config.h',
        'src/libusb/core.c',
        'src/libusb/descriptor.c',
        'src/libusb/hotplug.c',
        'src/libusb/hotplug.h',
        'src/libusb/interrupt.c',
        'src/libusb/interrupt.h',
        'src/libusb/io.c',
        'src/libusb/libusb.h',
        'src/libusb/libusbi.h',
        'src/libusb/strerror.c',
        'src/libusb/sync.c',
        'src/libusb/version.h',
        'src/libusb/version_nano.h',
      ],
      'include_dirs': [
        'src',
        'src/libusb',
        'src/libusb/os',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src/libusb',
        ],
      },
      'conditions': [
        [ 'OS == "linux" or OS == "android" or OS == "mac"', {
          'sources': [
            'src/libusb/os/poll_posix.c',
            'src/libusb/os/poll_posix.h',
            'src/libusb/os/threads_posix.c',
            'src/libusb/os/threads_posix.h',
          ],
          'defines': [
            'DEFAULT_VISIBILITY=',
            'HAVE_GETTIMEOFDAY=1',
            'HAVE_POLL_H=1',
            'HAVE_SYS_TIME_H=1',
            'LIBUSB_DESCRIBE="1.0.16"',
            'POLL_NFDS_TYPE=nfds_t',
            'THREADS_POSIX=1',
          ],
        }],
        [ 'OS == "linux" or OS == "android"', {
          'sources': [
            'src/libusb/os/linux_udev.c',
            'src/libusb/os/linux_usbfs.c',
            'src/libusb/os/linux_usbfs.h',
          ],
          'defines': [
            'HAVE_LIBUDEV=1',
            'OS_LINUX=1',
            'USE_UDEV=1',
            '_GNU_SOURCE=1',
          ],
        }],
        [ 'OS == "mac"', {
          'sources': [
            'src/libusb/os/darwin_usb.c',
            'src/libusb/os/darwin_usb.h',
          ],
          'defines': [
            'OS_DARWIN=1',
          ],
        }],
        [ 'OS == "win"', {
          'sources': [
            'src/libusb/os/poll_windows.c',
            'src/libusb/os/poll_windows.h',
            'src/libusb/os/threads_windows.c',
            'src/libusb/os/threads_windows.h',
            'src/libusb/os/windows_common.h',
            'src/libusb/os/windows_usb.c',
            'src/libusb/os/windows_usb.h',
            'src/msvc/config.h',
            'src/msvc/inttypes.h',
            'src/msvc/stdint.h',
          ],
          'include_dirs!': [
            'src',
          ],
          'include_dirs': [
            'src/msvc',
          ],
          'msvs_disabled_warnings': [ 4267 ],
        }],
      ],
    },
  ],
}
