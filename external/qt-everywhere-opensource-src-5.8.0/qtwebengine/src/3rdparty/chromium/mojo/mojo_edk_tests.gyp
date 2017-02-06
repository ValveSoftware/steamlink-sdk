# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'mojo_edk_tests',
      'type': 'none',
      'dependencies': [
        # NOTE: If adding a new dependency here, please consider whether it
        # should also be added to the list of Mojo-related dependencies of
        # build/all.gyp:All on iOS, as All cannot depend on the mojo_base
        # target on iOS due to the presence of the js targets, which cause v8
        # to be built.
        'mojo_message_pipe_perftests',
        'mojo_public_bindings_perftests',
        'mojo_public_bindings_unittests',
        'mojo_public_system_perftests',
        'mojo_public_system_unittests',
        'mojo_system_unittests',
        'mojo_js_unittests',
        'mojo_js_integration_tests',
      ],
    },
    {
      # GN version: //mojo/edk/test:mojo_public_bindings_unittests
      'target_name': 'mojo_public_bindings_unittests',
      'type': 'executable',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_public.gyp:mojo_public_bindings_test_utils',
        'mojo_public.gyp:mojo_public_test_utils',
        'mojo_public_tests.gyp:mojo_public_test_associated_interfaces',
        'mojo_public_tests.gyp:mojo_public_test_interfaces',
        'mojo_public_tests.gyp:mojo_public_test_interfaces_blink',
        'mojo_public_tests.gyp:mojo_public_test_interfaces_struct_traits',
      ],
      'variables': {
        'clang_warning_flags_unset': [ '-Wglobal-constructors' ],
      },
      'sources': [
        'public/cpp/bindings/tests/array_common_test.h',
        'public/cpp/bindings/tests/array_unittest.cc',
        'public/cpp/bindings/tests/associated_interface_unittest.cc',
        'public/cpp/bindings/tests/bind_task_runner_unittest.cc',
        'public/cpp/bindings/tests/binding_callback_unittest.cc',
        'public/cpp/bindings/tests/binding_unittest.cc',
        'public/cpp/bindings/tests/buffer_unittest.cc',
        'public/cpp/bindings/tests/connector_unittest.cc',
        'public/cpp/bindings/tests/constant_unittest.cc',
        'public/cpp/bindings/tests/container_test_util.cc',
        'public/cpp/bindings/tests/container_test_util.h',
        'public/cpp/bindings/tests/equals_unittest.cc',
        'public/cpp/bindings/tests/handle_passing_unittest.cc',
        'public/cpp/bindings/tests/interface_ptr_unittest.cc',
        'public/cpp/bindings/tests/map_common_test.h',
        'public/cpp/bindings/tests/map_unittest.cc',
        'public/cpp/bindings/tests/message_queue.cc',
        'public/cpp/bindings/tests/message_queue.h',
        'public/cpp/bindings/tests/multiplex_router_unittest.cc',
        'public/cpp/bindings/tests/pickle_unittest.cc',
        'public/cpp/bindings/tests/pickled_types_blink.cc',
        'public/cpp/bindings/tests/pickled_types_blink.h',
        'public/cpp/bindings/tests/pickled_types_chromium.cc',
        'public/cpp/bindings/tests/pickled_types_chromium.h',
        'public/cpp/bindings/tests/rect_blink.h',
        'public/cpp/bindings/tests/rect_blink_traits.h',
        'public/cpp/bindings/tests/rect_chromium.h',
        'public/cpp/bindings/tests/rect_chromium_traits.h',
        'public/cpp/bindings/tests/request_response_unittest.cc',
        'public/cpp/bindings/tests/router_test_util.cc',
        'public/cpp/bindings/tests/router_test_util.h',
        'public/cpp/bindings/tests/router_unittest.cc',
        'public/cpp/bindings/tests/sample_service_unittest.cc',
        'public/cpp/bindings/tests/serialization_warning_unittest.cc',
        'public/cpp/bindings/tests/stl_converters_unittest.cc',
        'public/cpp/bindings/tests/string_unittest.cc',
        'public/cpp/bindings/tests/struct_traits_unittest.cc',
        'public/cpp/bindings/tests/struct_unittest.cc',
        'public/cpp/bindings/tests/struct_with_traits_impl.cc',
        'public/cpp/bindings/tests/struct_with_traits_impl.h',
        'public/cpp/bindings/tests/struct_with_traits_impl_traits.cc',
        'public/cpp/bindings/tests/struct_with_traits_impl_traits.h',
        'public/cpp/bindings/tests/sync_method_unittest.cc',
        'public/cpp/bindings/tests/type_conversion_unittest.cc',
        'public/cpp/bindings/tests/union_unittest.cc',
        'public/cpp/bindings/tests/validation_context_unittest.cc',
        'public/cpp/bindings/tests/validation_unittest.cc',
        'public/cpp/bindings/tests/variant_test_util.h',
      ],
      'conditions': [
        # TODO(yzshen): Blink-flavor bindings tests should be moved into
        # mojo_public_bindings_for_blink_tests (which should eventually be moved
        # into blink).
        ['OS=="ios"', {
          'dependencies!': [
            'mojo_public.gyp:mojo_public_test_interfaces_blink',
          ],
          'sources!': [
            'public/cpp/bindings/tests/pickle_unittest.cc',
            'public/cpp/bindings/tests/pickled_types_blink.cc',
            'public/cpp/bindings/tests/pickled_types_blink.h',
            'public/cpp/bindings/tests/pickled_types_chromium.cc',
            'public/cpp/bindings/tests/pickled_types_chromium.h',
            'public/cpp/bindings/tests/rect_blink.h',
            'public/cpp/bindings/tests/rect_blink_traits.h',
            'public/cpp/bindings/tests/struct_traits_unittest.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/public/cpp/bindings/tests:for_blink_tests
      'target_name': 'mojo_public_bindings_for_blink_tests',
      'type': 'static_library',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_public_tests.gyp:mojo_public_test_interfaces',
        'mojo_public_tests.gyp:mojo_public_test_wtf_types',
        'mojo_public_tests.gyp:mojo_public_test_wtf_types_blink',
      ],
      'variables': {
         'clang_warning_flags_unset': [ '-Wglobal-constructors' ],
      },
      'sources': [
        'public/cpp/bindings/tests/array_common_test.h',
        'public/cpp/bindings/tests/container_test_util.cc',
        'public/cpp/bindings/tests/container_test_util.h',
        'public/cpp/bindings/tests/map_common_test.h',
        'public/cpp/bindings/tests/variant_test_util.h',
        'public/cpp/bindings/tests/wtf_array_unittest.cc',
        'public/cpp/bindings/tests/wtf_map_unittest.cc',
        'public/cpp/bindings/tests/wtf_types_unittest.cc',
      ],
    },
    {
      # GN version: //mojo/edk/test:mojo_public_bindings_perftests
      'target_name': 'mojo_public_bindings_perftests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        'mojo_base.gyp:mojo_common_lib',
        'mojo_edk.gyp:mojo_run_all_perftests',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_public.gyp:mojo_public_bindings_test_utils',
        'mojo_public.gyp:mojo_public_test_utils',
        'mojo_public_tests.gyp:mojo_public_test_interfaces',
      ],
      'sources': [
        'public/cpp/bindings/tests/bindings_perftest.cc',
        'public/cpp/bindings/tests/e2e_perftest.cc',
      ],
    },
    {
      # GN version: //mojo/public/cpp/system/tests:mojo_public_system_unittests
      # and         //mojo/public/c/system/tests
      'target_name': 'mojo_public_system_unittests',
      'type': 'executable',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_public.gyp:mojo_cpp_system',
        'mojo_public.gyp:mojo_public_test_utils',
      ],
      'sources': [
        '<(DEPTH)/mojo/public/c/system/tests/core_unittest.cc',
        '<(DEPTH)/mojo/public/c/system/tests/core_unittest_pure_c.c',
        '<(DEPTH)/mojo/public/c/system/tests/macros_unittest.cc',
        '<(DEPTH)/mojo/public/cpp/system/tests/core_unittest.cc',
        '<(DEPTH)/mojo/public/cpp/system/tests/watcher_unittest.cc',
      ],
    },
    {
      # GN version: //mojo/edk/test:mojo_public_system_perftests
      'target_name': 'mojo_public_system_perftests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_run_all_perftests',
        'mojo_public.gyp:mojo_public_system',
        'mojo_public.gyp:mojo_public_test_utils',
      ],
      'sources': [
        'public/c/system/tests/core_perftest.cc',
      ],
    },
    {
      # GN version: //mojo/edk/system:mojo_system_unittests
      'target_name': 'mojo_system_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_common_test_support',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_edk.gyp:mojo_system_impl',
        'mojo_edk.gyp:mojo_system_ports',
        'mojo_public.gyp:mojo_public_system',
      ],
      'sources': [
        'edk/embedder/embedder_unittest.cc',
        'edk/embedder/platform_channel_pair_posix_unittest.cc',
        'edk/embedder/platform_shared_buffer_unittest.cc',
        'edk/system/awakable_list_unittest.cc',
        'edk/system/core_test_base.cc',
        'edk/system/core_test_base.h',
        'edk/system/core_unittest.cc',
        'edk/system/message_pipe_unittest.cc',
        'edk/system/multiprocess_message_pipe_unittest.cc',
        'edk/system/options_validation_unittest.cc',
        'edk/system/platform_handle_dispatcher_unittest.cc',
        'edk/system/platform_wrapper_unittest.cc',
        'edk/system/ports/ports_unittest.cc',
        'edk/system/shared_buffer_dispatcher_unittest.cc',
        'edk/system/shared_buffer_unittest.cc',
        'edk/system/test_utils.cc',
        'edk/system/test_utils.h',
        'edk/system/wait_set_dispatcher_unittest.cc',
        'edk/system/waiter_test_utils.cc',
        'edk/system/waiter_test_utils.h',
        'edk/system/waiter_unittest.cc',
        'edk/system/watch_unittest.cc',
      ],
      'conditions': [
        ['OS=="ios"', {
          'sources!': [
            'edk/system/multiprocess_message_pipe_unittest.cc',
          ],
        }],
        ['OS == "android"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/edk/system:mojo_message_pipe_perftests
      'target_name': 'mojo_message_pipe_perftests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_common_test_support',
        'mojo_edk.gyp:mojo_run_all_perftests',
        'mojo_edk.gyp:mojo_system_impl',
        'mojo_public.gyp:mojo_public_system',
      ],
      'sources': [
        'edk/system/message_pipe_perftest.cc',
        'edk/system/test_utils.cc',
        'edk/system/test_utils.h',
      ],
    },
    # TODO(yzshen): fix the following two targets.
    {
      # GN version: //mojo/edk/js/test:js_unittests
      'target_name': 'mojo_js_unittests',
      'type': 'executable',
      'dependencies': [
        '../gin/gin.gyp:gin_test',
        'mojo_edk.gyp:mojo_common_test_support',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_edk.gyp:mojo_js_lib',
        'mojo_public_tests.gyp:mojo_public_test_interfaces',
      ],
      'sources': [
        'edk/js/handle_unittest.cc',
        'edk/js/test/run_js_tests.cc',
      ],
    },
    {
      # GN version: //mojo/edk/js/test:js_integration_tests
      'target_name': 'mojo_js_integration_tests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../gin/gin.gyp:gin_test',
        'mojo_base.gyp:mojo_common_lib',
        'mojo_edk.gyp:mojo_js_lib',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_js_to_cpp_bindings',
        'mojo_public_tests.gyp:mojo_public_test_interfaces',
      ],
      'sources': [
        'edk/js/test/run_js_integration_tests.cc',
        'edk/js/tests/js_to_cpp_tests.cc',
      ],
    },
    {
      'target_name': 'mojo_js_to_cpp_bindings',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'edk/js/tests/js_to_cpp.mojom',
        ],
      },
      'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'mojo_public_bindings_unittests_run',
          'type': 'none',
          'dependencies': [
            'mojo_public_bindings_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'mojo_public_bindings_unittests.isolate',
          ],
        },
        {
          'target_name': 'mojo_public_system_unittests_run',
          'type': 'none',
          'dependencies': [
            'mojo_public_system_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'mojo_public_system_unittests.isolate',
          ],
        },
        {
          'target_name': 'mojo_js_unittests_run',
          'type': 'none',
          'dependencies': [
            'mojo_js_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'mojo_js_unittests.isolate',
          ],
        },
        {
          'target_name': 'mojo_js_integration_tests_run',
          'type': 'none',
          'dependencies': [
            'mojo_js_integration_tests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'mojo_js_integration_tests.isolate',
          ],
        },
        {
          'target_name': 'mojo_system_unittests_run',
          'type': 'none',
          'dependencies': [
            'mojo_system_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'mojo_system_unittests.isolate',
          ],
        },
      ],
    }],
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'mojo_system_unittests_apk',
          'type': 'none',
          'dependencies': [
            'mojo_system_unittests',
          ],
          'variables': {
            'test_suite_name': 'mojo_system_unittests',
          },
          'includes': [ '../build/apk_test.gypi' ],
        },
      ],
    }],
  ],
}
