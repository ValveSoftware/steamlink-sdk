# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Essential components (and their tests) that are needed to build
# Chrome should be here.  Other components that are useful only in
# Mojo land like mojo_shell should be in mojo.gyp.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'mojo_base',
      'type': 'none',
      'dependencies': [
        # NOTE: If adding a new dependency here, please consider whether it
        # should also be added to the list of Mojo-related dependencies of
        # build/all.gyp:All on iOS, as All cannot depend on the mojo_base
        # target on iOS due to the presence of the js targets, which cause v8
        # to be built.
        'mojo_common_lib',
        'mojo_common_unittests',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            'mojo_public.gyp:mojo_bindings_java',
            'mojo_public.gyp:mojo_public_java',
          ],
        }],
      ]
    },
    {
      'target_name': 'mojo_none',
      'type': 'none',
    },
    {
      # GN version: //mojo/common
      'target_name': 'mojo_common_lib',
      'type': '<(component)',
      'defines': [
        'MOJO_COMMON_IMPLEMENTATION',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../mojo/mojo_public.gyp:mojo_public_system',
      ],
      'sources': [
        'common/common_type_converters.cc',
        'common/common_type_converters.h',
        'common/data_pipe_file_utils.cc',
        'common/data_pipe_utils.cc',
        'common/data_pipe_utils.h',
      ],
    },
    {
      # GN version: //mojo/common:common_custom_types
      'target_name': 'mojo_common_custom_types_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'common/common_custom_types.mojom',
        ],
        'mojom_typemaps': [
          'common/common_custom_types.typemap',
        ],
      },
      'dependencies': [
        '../ipc/ipc.gyp:ipc',
      ],
      'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
    },
    {
      # GN version: //mojo/common:test_common_custom_types
      'target_name': 'mojo_test_common_custom_types',
      'type': 'static_library',
      'variables': {
        'mojom_typemaps': [
          'common/common_custom_types.typemap',
        ],
      },
      'sources': [
        'common/test_common_custom_types.mojom',
      ],
      'dependencies': [
        'mojo_common_custom_types_mojom',
      ],
      'includes': [ 'mojom_bindings_generator.gypi' ],
    },
    {
      # GN version: //mojo/common:mojo_common_unittests
      'target_name': 'mojo_common_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../base/base.gyp:base_message_loop_tests',
        '../testing/gtest.gyp:gtest',
        '../url/url.gyp:url_lib',
        'mojo_common_custom_types_mojom',
        'mojo_common_lib',
        'mojo_test_common_custom_types',
        'mojo_edk.gyp:mojo_system_impl',
        'mojo_edk.gyp:mojo_common_test_support',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_public.gyp:mojo_public_test_utils',
      ],
      'sources': [
        'common/common_custom_types_unittest.cc',
        'common/common_type_converters_unittest.cc',
      ],
    },
  ],
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'mojo_jni_headers',
          'type': 'none',
          'dependencies': [
            'mojo_java_set_jni_headers',
          ],
          'sources': [
            'android/javatests/src/org/chromium/mojo/MojoTestCase.java',
            'android/javatests/src/org/chromium/mojo/bindings/ValidationTestUtil.java',
            'android/system/src/org/chromium/mojo/system/impl/CoreImpl.java',
          ],
          'variables': {
            'jni_gen_package': 'mojo',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'libmojo_system_java',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            'mojo_common_lib',
            'mojo_edk.gyp:mojo_system_impl',
            'mojo_jni_headers',
            'mojo_public.gyp:mojo_message_pump_lib',
          ],
          'sources': [
            'android/system/core_impl.cc',
            'android/system/core_impl.h',
          ],
        },
        {
          'target_name': 'mojo_java_set_jni_headers',
          'type': 'none',
          'variables': {
            'jni_gen_package': 'mojo',
            'input_java_class': 'java/util/HashSet.class',
          },
          'includes': [ '../build/jar_file_jni_generator.gypi' ],
        },
        {
          'target_name': 'mojo_system_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java',
            'libmojo_system_java',
            'mojo_public.gyp:mojo_public_java',
          ],
          'variables': {
            'java_in_dir': '<(DEPTH)/mojo/android/system',
          },
          'includes': [ '../build/java.gypi' ],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'mojo_common_unittests_run',
          'type': 'none',
          'dependencies': [
            'mojo_common_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'mojo_common_unittests.isolate',
          ],
        },
      ],
    }],
  ]
}
