# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    'mojo_edk.gypi',
  ],
  'target_defaults' : {
    'include_dirs': [
      '..',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        '..',
      ],
    },
  },
  'targets': [
    {
      # GN version: //mojo/edk/system/ports
      'target_name': 'mojo_system_ports',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
      ],
      'sources': [
        '<@(mojo_edk_ports_sources)',
      ],
    },
    {
      # GN version: //mojo/edk/system
      'target_name': 'mojo_system_impl',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
        'mojo_public.gyp:mojo_public_system',
        'mojo_system_ports',
      ],
      'defines': [
        'MOJO_SYSTEM_IMPL_IMPLEMENTATION',
      ],
      'sources': [
        '<@(mojo_edk_system_impl_sources)',
        '<@(mojo_edk_system_impl_non_nacl_sources)',
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            '../third_party/ashmem/ashmem.gyp:ashmem',
          ],
        }],
        ['OS=="android" or chromeos==1', {
          'defines': [
            'MOJO_EDK_LEGACY_PROTOCOL',
          ],
        }],
        ['OS=="win"', {
           # Structure was padded due to __declspec(align()), which is
           # uninteresting.
          'msvs_disabled_warnings': [ 4324 ],
        }],
        ['OS=="mac" and OS!="ios"', {
          'sources': [
            'edk/system/mach_port_relay.cc',
            'edk/system/mach_port_relay.h',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/edk/js
      'target_name': 'mojo_js_lib',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../gin/gin.gyp:gin',
        '../v8/src/v8.gyp:v8',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../gin/gin.gyp:gin',
      ],
      'sources': [
        # Sources list duplicated in GN build.
        'edk/js/core.cc',
        'edk/js/core.h',
        'edk/js/drain_data.cc',
        'edk/js/drain_data.h',
        'edk/js/handle.cc',
        'edk/js/handle.h',
        'edk/js/handle_close_observer.h',
        'edk/js/mojo_runner_delegate.cc',
        'edk/js/mojo_runner_delegate.h',
        'edk/js/support.cc',
        'edk/js/support.h',
        'edk/js/threading.cc',
        'edk/js/threading.h',
        'edk/js/waiting_callback.cc',
        'edk/js/waiting_callback.h',
      ],
    },
    {
      # GN version: //mojo/edk/test:test_support_impl
      'target_name': 'mojo_test_support_impl',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'edk/test/test_support_impl.cc',
        'edk/test/test_support_impl.h',
      ],
    },
    {
      # GN version: //mojo/edk/test:test_support
      'target_name': 'mojo_common_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        'mojo_system_impl',
      ],
      'sources': [
        'edk/test/mojo_test_base.cc',
        'edk/test/mojo_test_base.h',
        'edk/test/multiprocess_test_helper.cc',
        'edk/test/multiprocess_test_helper.h',
        'edk/test/scoped_ipc_support.cc',
        'edk/test/scoped_ipc_support.h',
        'edk/test/test_utils.h',
        'edk/test/test_utils_posix.cc',
        'edk/test/test_utils_win.cc',
      ],
      'conditions': [
        ['OS=="ios"', {
          'sources!': [
            'edk/test/multiprocess_test_helper.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/edk/test:run_all_unittests
      'target_name': 'mojo_run_all_unittests',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        'mojo_common_test_support',
        'mojo_public.gyp:mojo_public_test_support',
        'mojo_system_impl',
        'mojo_test_support_impl',
      ],
      'sources': [
        'edk/test/run_all_unittests.cc',
      ],
    },
    {
      # GN version: //mojo/edk/test:run_all_perftests
      'target_name': 'mojo_run_all_perftests',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        'mojo_common_test_support',
        'mojo_public.gyp:mojo_public_test_support',
        'mojo_system_impl',
        'mojo_test_support_impl',
      ],
      'sources': [
        'edk/test/run_all_perftests.cc',
      ],
    },
  ],
  'conditions': [
    ['OS == "win" and target_arch=="ia32"', {
      'targets': [
        {
          # GN version: //mojo/edk/system/ports
          'target_name': 'mojo_system_ports_win64',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base_win64',
            '../crypto/crypto.gyp:crypto_nacl_win64',
          ],
          'sources': [
            '<@(mojo_edk_ports_sources)',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
        {
          # GN version: //mojo/edk/system
          'target_name': 'mojo_system_impl_win64',
          'type': '<(component)',
          'dependencies': [
            '../base/base.gyp:base_win64',
            '../crypto/crypto.gyp:crypto_nacl_win64',
            'mojo_public.gyp:mojo_public_system_win64',
            'mojo_system_ports_win64',
          ],
          'defines': [
            'MOJO_SYSTEM_IMPL_IMPLEMENTATION',
          ],
          'sources': [
            '<@(mojo_edk_system_impl_sources)',
            '<@(mojo_edk_system_impl_non_nacl_sources)',
          ],
          # Structure was padded due to __declspec(align()), which is
          # uninteresting.
          'msvs_disabled_warnings': [ 4324 ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ]
}
