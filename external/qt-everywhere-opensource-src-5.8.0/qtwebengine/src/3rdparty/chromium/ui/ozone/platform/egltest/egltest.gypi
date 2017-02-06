# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'internal_ozone_platform_deps': [
      'ozone_platform_egltest',
    ],
    'internal_ozone_platforms': [
      'egltest'
    ],
  },
  'targets': [
    {
      'target_name': 'ozone_platform_egltest',
      'type': 'static_library',
      'defines': [
        'OZONE_IMPLEMENTATION',
      ],
      'dependencies': [
        'ozone.gyp:ozone_base',
        'ozone.gyp:ozone_common',
        '../../base/base.gyp:base',
        '../../third_party/khronos/khronos.gyp:khronos_headers',
        '../events/devices/events_devices.gyp:events_devices',
        '../events/events.gyp:events',
        '../events/ozone/events_ozone.gyp:events_ozone_evdev',
        '../events/ozone/events_ozone.gyp:events_ozone_layout',
        '../events/platform/events_platform.gyp:events_platform',
        '../gfx/gfx.gyp:gfx',
        'eglplatform_shim',
      ],
      'sources': [
        'client_native_pixmap_factory_egltest.cc',
        'client_native_pixmap_factory_egltest.h',
        'ozone_platform_egltest.cc',
        'ozone_platform_egltest.h',
      ],
    },
    {
      'target_name': 'eglplatform_shim',
      'type': 'static_library',
      'dependencies': [
        '../../third_party/khronos/khronos.gyp:khronos_headers',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'include_dirs': [
        '../../../..',
      ],
      'hard_dependency': 1,
      'actions': [
        {
          'variables': {
            'output_cc': '<(INTERMEDIATE_DIR)/libeglplatform_shim_loader.cc',
            'output_h': '<(SHARED_INTERMEDIATE_DIR)/library_loaders/libeglplatform_shim.h',
            'generator': '../../tools/generate_library_loader/generate_library_loader.py',
          },
          'action_name': 'generate_libeglplatform_shim_loader',
          'inputs': [
            '<(generator)',
          ],
          'outputs': [
            '<(output_h)',
            '<(output_cc)',
          ],
          'action': ['python',
                     '<(generator)',
                     '--name', 'LibeglplatformShimLoader',
                     '--output-h', '<(output_h)',
                     '--output-cc', '<(output_cc)',
                     '--header', '"ui/ozone/platform/egltest/eglplatform_shim.h"',
                     'ShimQueryString',
                     'ShimInitialize',
                     'ShimTerminate',
                     'ShimCreateWindow',
                     'ShimQueryWindow',
                     'ShimDestroyWindow',
                     'ShimGetNativeDisplay',
                     'ShimGetNativeWindow',
                     'ShimReleaseNativeWindow',
          ],
          'message': 'Generating libeglplatform_shim library loader',
          'process_outputs_as_sources': 1,
        },
      ],
    },
  ],
  'conditions': [
    ['ozone_platform_ozonex == 1', {
      'targets': [
        {
          'target_name': 'eglplatform_shim_x11',
          'type': 'loadable_module',
          'product_name': 'eglplatform_shim',
          'product_extension': 'so.1',
          'include_dirs': [
            '../../../..',
          ],
          'dependencies': [
            '../../build/linux/system.gyp:x11',
          ],
          'sources': [
            'eglplatform_shim.h',
            'eglplatform_shim_xeleven.cc',
          ],
        },
      ],
    }],
  ],
}
