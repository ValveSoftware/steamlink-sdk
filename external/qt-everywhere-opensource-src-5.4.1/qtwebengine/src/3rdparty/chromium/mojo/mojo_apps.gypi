{
  'targets': [
    {
      'target_name': 'mojo_js_lib',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../gin/gin.gyp:gin',
        '../ui/gl/gl.gyp:gl',
        '../v8/tools/gyp/v8.gyp:v8',
        'mojo_common_lib',
        'mojo_environment_chromium',
        'mojo_gles2',
        'mojo_gles2_bindings',
        'mojo_js_bindings_lib',
        'mojo_native_viewport_bindings',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../gin/gin.gyp:gin',
        'mojo_common_lib',
        'mojo_gles2',
        'mojo_gles2_bindings',
        'mojo_native_viewport_bindings',
      ],
      'sources': [
        'apps/js/mojo_runner_delegate.cc',
        'apps/js/mojo_runner_delegate.h',
        'apps/js/bindings/threading.cc',
        'apps/js/bindings/threading.h',
        'apps/js/bindings/gl/context.cc',
        'apps/js/bindings/gl/context.h',
        'apps/js/bindings/gl/module.cc',
        'apps/js/bindings/gl/module.h',
        'apps/js/bindings/monotonic_clock.cc',
        'apps/js/bindings/monotonic_clock.h',
      ],
    },
    {
      'target_name': 'mojo_apps_js_bindings',
      'type': 'static_library',
      'sources': [
        'apps/js/test/js_to_cpp.mojom',
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
      'target_name': 'mojo_apps_js_unittests',
      'type': 'executable',
      'dependencies': [
        '../gin/gin.gyp:gin_test',
        'mojo_apps_js_bindings',
        'mojo_common_lib',
        'mojo_common_test_support',
        'mojo_js_lib',
        'mojo_run_all_unittests',
        'mojo_public_test_interfaces',
      ],
      'sources': [
        'apps/js/test/js_to_cpp_unittest.cc',
        'apps/js/test/run_apps_js_tests.cc',
      ],
    },
    {
      'target_name': 'mojo_js',
      'type': 'shared_library',
      'dependencies': [
        'mojo_js_lib',
        'mojo_system_impl',
      ],
      'sources': [
        'apps/js/main.cc',
      ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'mojo_apps_js_unittests_run',
          'type': 'none',
          'dependencies': [
            'mojo_apps_js_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
            'mojo_apps_js_unittests.isolate',
          ],
          'sources': [
            'mojo_apps_js_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
