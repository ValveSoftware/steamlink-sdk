# Copyright 2014 The Chroium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojo_public.gypi',
  ],
  'variables': {
    'chromium_code': 1,
  },
  'target_defaults' : {
    'include_dirs': [
      '..',
    ],
  },
  'targets': [
    {
      'target_name': 'mojo_public',
      'type': 'none',
      'dependencies': [
        'mojo_js_bindings',
        'mojo_public_system',
      ],
    },
    {
      # GN version: //mojo/public/c/system
      'target_name': 'mojo_public_system',
      'type': '<(component)',
      'sources': [
        '<@(mojo_public_system_sources)',
      ],
      'defines': [
        'MOJO_SYSTEM_IMPLEMENTATION',
      ],
    },
    {
      # GN version: //mojo/public/cpp/system
      'target_name': 'mojo_cpp_system',
      'type': 'static_library',
      'sources': [
        '<@(mojo_cpp_system_sources)',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'mojo_public_system',
      ],
    },
    {
      # GN version: //mojo/public/cpp/bindings
      'target_name': 'mojo_cpp_bindings',
      'type': 'static_library',
      'include_dirs': [
        '..'
      ],
      'sources': [
        '<@(mojo_cpp_bindings_sources)',

        # This comes from the mojo_interface_bindings_cpp_sources dependency.
        '>@(mojom_generated_sources)',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'mojo_cpp_system',
        'mojo_interface_bindings_cpp_sources',
      ],
    },
    {
      # GN version: //mojo/message_pump
      'target_name': 'mojo_message_pump_lib',
      'type': '<(component)',
      'defines': [
        'MOJO_MESSAGE_PUMP_IMPLEMENTATION',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'mojo_cpp_system',
      ],
      'sources': [
        'message_pump/handle_watcher.cc',
        'message_pump/handle_watcher.h',
        'message_pump/message_pump_mojo.cc',
        'message_pump/message_pump_mojo.h',
        'message_pump/message_pump_mojo_handler.h',
        'message_pump/time_helper.cc',
        'message_pump/time_helper.h',
      ],
    },
    {
      # GN version: //mojo/public/js
      'target_name': 'mojo_js_bindings',
      'type': 'static_library',
      'include_dirs': [
        '..'
      ],
      'sources': [
        'public/js/constants.cc',
        'public/js/constants.h',
      ],
    },
    {
      'target_name': 'mojo_interface_bindings_mojom',
      'type': 'none',
      'variables': {
        'require_interface_bindings': 0,
        'mojom_files': [
          'public/interfaces/bindings/interface_control_messages.mojom',
          'public/interfaces/bindings/pipe_control_messages.mojom',
        ],
      },
      'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
    },
    {
      'target_name': 'mojo_interface_bindings_cpp_sources',
      'type': 'none',
      'dependencies': [
        'mojo_interface_bindings_mojom',
      ],
    },
    {
      # This target can be used to introduce a dependency on interface bindings
      # generation without introducing any side-effects in the dependent
      # target's configuration.
      'target_name': 'mojo_interface_bindings_generation',
      'type': 'none',
      'dependencies': [
        'mojo_interface_bindings_cpp_sources',
      ],
    },
    {
      # GN version: //mojo/public/c/test_support
      'target_name': 'mojo_public_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'sources': [
        'public/c/test_support/test_support.h',
        # TODO(vtl): Convert this to thunks http://crbug.com/386799
        'public/tests/test_support_private.cc',
        'public/tests/test_support_private.h',
      ],
    },
    {
      # GN version: //mojo/public/cpp/test_support:test_utils
      'target_name': 'mojo_public_test_utils',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_public_test_support',
      ],
      'sources': [
        'public/cpp/test_support/lib/test_support.cc',
        'public/cpp/test_support/lib/test_utils.cc',
        'public/cpp/test_support/test_utils.h',
      ],
    },
    {
      # GN version: //mojo/public/cpp/bindings/tests:mojo_public_bindings_test_utils
      'target_name': 'mojo_public_bindings_test_utils',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'public/cpp/bindings/tests/validation_test_input_parser.cc',
        'public/cpp/bindings/tests/validation_test_input_parser.h',
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          # GN version: //mojo/public/java:system
          'target_name': 'mojo_public_java',
          'type': 'none',
          'variables': {
            'chromium_code': 0,
            'java_in_dir': 'public/java/system',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'mojo_interface_bindings_java_sources',
          'type': 'none',
          'dependencies': [
            'mojo_interface_bindings_mojom',
          ],
        },
        {
          # GN version: //mojo/public/java:bindings
          'target_name': 'mojo_bindings_java',
          'type': 'none',
          'variables': {
            'chromium_code': 0,
            'java_in_dir': 'public/java/bindings',
           },
           'dependencies': [
             'mojo_interface_bindings_java_sources',
             'mojo_public_java',
             '<(DEPTH)/base/base.gyp:base_java',
           ],
           'includes': [ '../build/java.gypi' ],
        },
      ],
    }],
    ['OS != "ios"', {
      'targets': [
        {
          # TODO(yzshen): crbug.com/617718 Consider moving this into blink.
          # GN version: //mojo/public/cpp/bindings:wtf_support
          'target_name': 'mojo_cpp_bindings_wtf_support',
          'type': 'static_library',
          'include_dirs': [
            '..'
          ],
          'sources': [
            'public/cpp/bindings/array_traits_wtf.h',
            'public/cpp/bindings/array_traits_wtf_vector.h',
            'public/cpp/bindings/lib/string_traits_wtf.cc',
            'public/cpp/bindings/lib/wtf_serialization.h',
            'public/cpp/bindings/map_traits_wtf.h',
            'public/cpp/bindings/string_traits_wtf.h',
            'public/cpp/bindings/wtf_array.h',
            'public/cpp/bindings/wtf_map.h',
          ],
          'dependencies': [
            'mojo_cpp_bindings',
            '../third_party/WebKit/Source/config.gyp:config',
            '../third_party/WebKit/Source/wtf/wtf.gyp:wtf',
          ],
          'export_dependent_settings': [
            'mojo_cpp_bindings',
            '../third_party/WebKit/Source/config.gyp:config',
          ],
        },
      ],
    }],
    ['OS == "win" and target_arch=="ia32"', {
      'targets': [
        {
          # GN version: //mojo/public/c/system
          'target_name': 'mojo_public_system_win64',
          'type': '<(component)',
          'sources': [
            '<@(mojo_public_system_sources)',
          ],
          'defines': [
            'MOJO_SYSTEM_IMPLEMENTATION',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
        {
          # GN version: //mojo/public/cpp/system
          'target_name': 'mojo_cpp_system_win64',
          'type': 'static_library',
          'sources': [
            '<@(mojo_cpp_system_sources)',
          ],
          'dependencies': [
            '../base/base.gyp:base_win64',
            'mojo_public_system_win64',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
        {
          # GN version: //mojo/public/cpp/bindings
          'target_name': 'mojo_cpp_bindings_win64',
          'type': 'static_library',
          'include_dirs': [
            '..'
          ],
          'sources': [
            '<@(mojo_cpp_bindings_sources)',

            # This comes from the mojo_interface_bindings_cpp_sources dependency.
            '>@(mojom_generated_sources)',
          ],
          'dependencies': [
            '../base/base.gyp:base_win64',
            'mojo_cpp_system_win64',
            'mojo_interface_bindings_cpp_sources',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}
