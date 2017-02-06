# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //device/vibration:mojo_bindings
      'target_name': 'device_vibration_mojo_bindings',
      'type': 'static_library',
      'includes': [
        '../../mojo/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'vibration_manager.mojom',
      ],
    },
    {
      # GN version: //device/vibration:mojo_bindings_blink
      'target_name': 'device_vibration_mojo_bindings_for_blink',
      'type': 'static_library',
      'variables': {
        'for_blink': 'true',
      },
      'includes': [
        '../../mojo/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'vibration_manager.mojom',
      ],
    },
    {
      # GN version: //device/vibration
      'target_name': 'device_vibration',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../mojo/mojo_edk.gyp:mojo_system_impl',
        '../../mojo/mojo_public.gyp:mojo_cpp_bindings',
        'device_vibration_mojo_bindings',
      ],
      'defines': [
        'DEVICE_VIBRATION_IMPLEMENTATION',
      ],
      'sources': [
        'vibration_manager_impl.h',
        'vibration_manager_impl_default.cc',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            'device_vibration_java',
          ],
          'sources!': [
            'vibration_manager_impl_default.cc',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'device_vibration_java',
          'type': 'none',
          'dependencies': [
            '../../base/base.gyp:base',
            '../../mojo/mojo_public.gyp:mojo_bindings_java',
            'device_vibration_mojo_bindings',
          ],
          'variables': {
            'java_in_dir': '../../device/vibration/android/java',
          },
          'includes': [ '../../build/java.gypi' ],
        },
      ],
    }],
  ],
}
