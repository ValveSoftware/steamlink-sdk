{
  'targets': [
    {
      'target_name': 'mojo_system',
      'type': 'static_library',
      'defines': [
        'MOJO_SYSTEM_IMPLEMENTATION',
      ],
      'include_dirs': [
        '..',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'all_dependent_settings': {
        'conditions': [
          # We need to be able to call the MojoSetSystemThunks() function in
          # system_thunks.cc
          ['OS=="android"', {
            'ldflags!': [
              '-Wl,--exclude-libs=ALL',
            ],
          }],
        ],
      },
      'sources': [
        'public/c/system/buffer.h',
        'public/c/system/core.h',
        'public/c/system/data_pipe.h',
        'public/c/system/functions.h',
        'public/c/system/macros.h',
        'public/c/system/message_pipe.h',
        'public/c/system/system_export.h',
        'public/c/system/types.h',
        'public/platform/native/system_thunks.cc',
        'public/platform/native/system_thunks.h',
      ],
    },
    {
      'target_name': 'mojo_gles2',
      'type': 'shared_library',
      'defines': [
        'MOJO_GLES2_IMPLEMENTATION',
        'GLES2_USE_MOJO',
      ],
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../third_party/khronos/khronos.gyp:khronos_headers'
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
        'defines': [
          'GLES2_USE_MOJO',
        ],
      },
      'sources': [
        'public/c/gles2/gles2.h',
        'public/c/gles2/gles2_export.h',
        'public/gles2/gles2_private.cc',
        'public/gles2/gles2_private.h',
      ],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            # Make it a run-path dependent library.
            'DYLIB_INSTALL_NAME_BASE': '@loader_path',
          },
        }],
      ],
    },
    {
      'target_name': 'mojo_test_support',
      'type': 'shared_library',
      'defines': [
        'MOJO_TEST_SUPPORT_IMPLEMENTATION',
      ],
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
        'public/c/test_support/test_support_export.h',
        'public/tests/test_support_private.cc',
        'public/tests/test_support_private.h',
      ],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            # Make it a run-path dependent library.
            'DYLIB_INSTALL_NAME_BASE': '@loader_path',
          },
        }],
      ],
    },
    {
      'target_name': 'mojo_public_test_utils',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_test_support',
      ],
      'sources': [
        'public/cpp/test_support/lib/test_support.cc',
        'public/cpp/test_support/lib/test_utils.cc',
        'public/cpp/test_support/test_utils.h',
      ],
    },
    # TODO(vtl): Reorganize the mojo_public_*_unittests.
    {
      'target_name': 'mojo_public_bindings_unittests',
      'type': 'executable',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'mojo_cpp_bindings',
        'mojo_environment_standalone',
        'mojo_public_test_utils',
        'mojo_run_all_unittests',
        'mojo_public_test_interfaces',
        'mojo_utility',
      ],
      'sources': [
        'public/cpp/bindings/tests/array_unittest.cc',
        'public/cpp/bindings/tests/bounds_checker_unittest.cc',
        'public/cpp/bindings/tests/buffer_unittest.cc',
        'public/cpp/bindings/tests/connector_unittest.cc',
        'public/cpp/bindings/tests/handle_passing_unittest.cc',
        'public/cpp/bindings/tests/interface_ptr_unittest.cc',
        'public/cpp/bindings/tests/request_response_unittest.cc',
        'public/cpp/bindings/tests/router_unittest.cc',
        'public/cpp/bindings/tests/sample_service_unittest.cc',
        'public/cpp/bindings/tests/string_unittest.cc',
        'public/cpp/bindings/tests/struct_unittest.cc',
        'public/cpp/bindings/tests/type_conversion_unittest.cc',
        'public/cpp/bindings/tests/validation_test_input_parser.cc',
        'public/cpp/bindings/tests/validation_test_input_parser.h',
        'public/cpp/bindings/tests/validation_unittest.cc',
      ],
    },
    {
      'target_name': 'mojo_public_environment_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_environment_standalone',
        'mojo_public_test_utils',
        'mojo_run_all_unittests',
        'mojo_utility',
      ],
      'sources': [
        'public/cpp/environment/tests/async_waiter_unittest.cc',
        'public/cpp/environment/tests/logger_unittest.cc',
        'public/cpp/environment/tests/logging_unittest.cc',
      ],
    },
    {
      'target_name': 'mojo_public_system_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_cpp_bindings',
        'mojo_public_test_utils',
        'mojo_run_all_unittests',
      ],
      'sources': [
        'public/c/system/tests/core_unittest.cc',
        'public/c/system/tests/core_unittest_pure_c.c',
        'public/c/system/tests/macros_unittest.cc',
        'public/cpp/system/tests/core_unittest.cc',
        'public/cpp/system/tests/macros_unittest.cc',
      ],
    },
    {
      'target_name': 'mojo_public_utility_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_cpp_bindings',
        'mojo_public_test_utils',
        'mojo_run_all_unittests',
        'mojo_utility',
      ],
      'sources': [
        'public/cpp/utility/tests/mutex_unittest.cc',
        'public/cpp/utility/tests/run_loop_unittest.cc',
        'public/cpp/utility/tests/thread_unittest.cc',
      ],
      'conditions': [
        # See crbug.com/342893:
        ['OS=="win"', {
          'sources!': [
            'public/cpp/utility/tests/mutex_unittest.cc',
            'public/cpp/utility/tests/thread_unittest.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'mojo_public_system_perftests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_public_test_utils',
        'mojo_run_all_perftests',
        'mojo_utility',
      ],
      'sources': [
        'public/c/system/tests/core_perftest.cc',
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
        'public/cpp/bindings/array.h',
        'public/cpp/bindings/callback.h',
        'public/cpp/bindings/error_handler.h',
        'public/cpp/bindings/interface_impl.h',
        'public/cpp/bindings/interface_ptr.h',
        'public/cpp/bindings/interface_request.h',
        'public/cpp/bindings/message.h',
        'public/cpp/bindings/message_filter.h',
        'public/cpp/bindings/no_interface.h',
        'public/cpp/bindings/string.h',
        'public/cpp/bindings/sync_dispatcher.h',
        'public/cpp/bindings/type_converter.h',
        'public/cpp/bindings/lib/array_internal.h',
        'public/cpp/bindings/lib/array_internal.cc',
        'public/cpp/bindings/lib/array_serialization.h',
        'public/cpp/bindings/lib/bindings_internal.h',
        'public/cpp/bindings/lib/bindings_serialization.cc',
        'public/cpp/bindings/lib/bindings_serialization.h',
        'public/cpp/bindings/lib/bounds_checker.cc',
        'public/cpp/bindings/lib/bounds_checker.h',
        'public/cpp/bindings/lib/buffer.h',
        'public/cpp/bindings/lib/callback_internal.h',
        'public/cpp/bindings/lib/connector.cc',
        'public/cpp/bindings/lib/connector.h',
        'public/cpp/bindings/lib/filter_chain.cc',
        'public/cpp/bindings/lib/filter_chain.h',
        'public/cpp/bindings/lib/fixed_buffer.cc',
        'public/cpp/bindings/lib/fixed_buffer.h',
        'public/cpp/bindings/lib/interface_impl_internal.h',
        'public/cpp/bindings/lib/interface_ptr_internal.h',
        'public/cpp/bindings/lib/message.cc',
        'public/cpp/bindings/lib/message_builder.cc',
        'public/cpp/bindings/lib/message_builder.h',
        'public/cpp/bindings/lib/message_filter.cc',
        'public/cpp/bindings/lib/message_header_validator.cc',
        'public/cpp/bindings/lib/message_header_validator.h',
        'public/cpp/bindings/lib/message_internal.h',
        'public/cpp/bindings/lib/message_queue.cc',
        'public/cpp/bindings/lib/message_queue.h',
        'public/cpp/bindings/lib/no_interface.cc',
        'public/cpp/bindings/lib/router.cc',
        'public/cpp/bindings/lib/router.h',
        'public/cpp/bindings/lib/shared_data.h',
        'public/cpp/bindings/lib/shared_ptr.h',
        'public/cpp/bindings/lib/string_serialization.h',
        'public/cpp/bindings/lib/string_serialization.cc',
        'public/cpp/bindings/lib/sync_dispatcher.cc',
        'public/cpp/bindings/lib/validation_errors.cc',
        'public/cpp/bindings/lib/validation_errors.h',
      ],
    },
    {
      # GN version: //mojo/public/js/bindings
      'target_name': 'mojo_js_bindings',
      'type': 'static_library',
      'include_dirs': [
        '..'
      ],
      'sources': [
        'public/js/bindings/constants.cc',
        'public/js/bindings/constants.h',
      ],
    },
    {
      'target_name': 'mojo_public_test_interfaces',
      'type': 'static_library',
      'sources': [
        'public/interfaces/bindings/tests/math_calculator.mojom',
        'public/interfaces/bindings/tests/sample_factory.mojom',
        'public/interfaces/bindings/tests/sample_import.mojom',
        'public/interfaces/bindings/tests/sample_import2.mojom',
        'public/interfaces/bindings/tests/sample_interfaces.mojom',
        'public/interfaces/bindings/tests/sample_service.mojom',
        'public/interfaces/bindings/tests/test_structs.mojom',
        'public/interfaces/bindings/tests/validation_test_interfaces.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_cpp_bindings',
      ],
    },
    {
      'target_name': 'mojo_environment_standalone',
      'type': 'static_library',
      'sources': [
        'public/c/environment/async_waiter.h',
        'public/c/environment/logger.h',
        'public/c/environment/logging.h',
        'public/cpp/environment/environment.h',
        'public/cpp/environment/lib/default_async_waiter.cc',
        'public/cpp/environment/lib/default_async_waiter.h',
        'public/cpp/environment/lib/default_logger.cc',
        'public/cpp/environment/lib/default_logger.h',
        'public/cpp/environment/lib/environment.cc',
        'public/cpp/environment/lib/logging.cc',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      'target_name': 'mojo_utility',
      'type': 'static_library',
      'sources': [
        'public/cpp/utility/mutex.h',
        'public/cpp/utility/run_loop.h',
        'public/cpp/utility/run_loop_handler.h',
        'public/cpp/utility/thread.h',
        'public/cpp/utility/lib/mutex.cc',
        'public/cpp/utility/lib/run_loop.cc',
        'public/cpp/utility/lib/thread.cc',
        'public/cpp/utility/lib/thread_local.h',
        'public/cpp/utility/lib/thread_local_posix.cc',
        'public/cpp/utility/lib/thread_local_win.cc',
      ],
      'conditions': [
        # See crbug.com/342893:
        ['OS=="win"', {
          'sources!': [
            'public/cpp/utility/mutex.h',
            'public/cpp/utility/thread.h',
            'public/cpp/utility/lib/mutex.cc',
            'public/cpp/utility/lib/thread.cc',
          ],
        }],
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      # GN version: //mojo/public/interfaces/interface_provider:interface_provider
      'target_name': 'mojo_interface_provider_bindings',
      'type': 'static_library',
      'sources': [
        'public/interfaces/interface_provider/interface_provider.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/public/interfaces/service_provider:service_provider
      'target_name': 'mojo_service_provider_bindings',
      'type': 'static_library',
      'sources': [
        'public/interfaces/service_provider/service_provider.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
    },
    {
      'target_name': 'mojo_application',
      'type': 'static_library',
      'sources': [
        'public/cpp/application/application.h',
        'public/cpp/application/connect.h',
        'public/cpp/application/lib/application.cc',
        'public/cpp/application/lib/service_connector.cc',
        'public/cpp/application/lib/service_connector.h',
        'public/cpp/application/lib/service_registry.cc',
        'public/cpp/application/lib/service_registry.h',
      ],
      'dependencies': [
        'mojo_service_provider_bindings',
      ],
      'export_dependent_settings': [
        'mojo_service_provider_bindings',
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'mojo_public_java',
          'type': 'none',
          'variables': {
            'java_in_dir': 'public/java',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'mojo_bindings_java',
          'type': 'none',
          'variables': {
            'java_in_dir': 'bindings/java',
          },
          'dependencies': [
            'mojo_public_java',
          ],
          'includes': [ '../build/java.gypi' ],
        },
      ],
    }],
  ],
}
