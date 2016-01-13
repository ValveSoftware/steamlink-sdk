# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //device/serial
      'target_name': 'device_serial',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../../build/linux/system.gyp:udev',
          ],
        }],
      ],
      'variables': {
        'mojom_base_output_dir': 'device/serial',
      },
      'includes': [
        '../../mojo/public/tools/bindings/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'serial.mojom',
        'serial_device_enumerator.cc',
        'serial_device_enumerator.h',
        'serial_device_enumerator_linux.cc',
        'serial_device_enumerator_linux.h',
        'serial_device_enumerator_mac.cc',
        'serial_device_enumerator_mac.h',
        'serial_device_enumerator_win.cc',
        'serial_device_enumerator_win.h',
      ],
    },
  ],
}
