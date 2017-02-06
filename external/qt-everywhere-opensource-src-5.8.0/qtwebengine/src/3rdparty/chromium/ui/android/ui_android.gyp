# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/android
      'target_name': 'ui_android',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../cc/cc.gyp:cc',
        '../../skia/skia.gyp:skia',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        'ui_android_jni_headers',
      ],
      'defines': [
        'UI_ANDROID_IMPLEMENTATION',
      ],
      'sources' : [
        'animation_utils.h',
        'edge_effect.cc',
        'edge_effect.h',
        'edge_effect_base.cc',
        'edge_effect_base.h',
        'edge_effect_l.cc',
        'edge_effect_l.h',
        'overscroll_glow.cc',
        'overscroll_glow.h',
        'overscroll_refresh.cc',
        'overscroll_refresh.h',
        'resources/crushed_sprite_resource.cc',
        'resources/crushed_sprite_resource.h',
        'resources/resource_manager.cc',
        'resources/resource_manager.h',
        'resources/resource_manager_impl.cc',
        'resources/resource_manager_impl.h',
        'resources/ui_resource_provider.h',
        'ui_android_export.h',
        'ui_android_jni_registrar.cc',
        'ui_android_jni_registrar.h',
        'view_android.h',
        'window_android.cc',
        'window_android.h',
        'window_android_compositor.h',
        'window_android_observer.h',
      ],
    },
    {
      'target_name': 'ui_android_jni_headers',
      'type': 'none',
      'sources': [
        'java/src/org/chromium/ui/base/WindowAndroid.java',
        'java/src/org/chromium/ui/resources/ResourceManager.java',
      ],
      'variables': {
        'jni_gen_package': 'ui_android',
      },
      'includes': [ '../../build/jni_generator.gypi' ],
    },
    {
      'target_name': 'android_resource_type_java',
      'type': 'none',
      'variables': {
        'source_file': 'resources/resource_manager.h',
      },
      'includes': [ '../../build/android/java_cpp_enum.gypi' ],
    },
    {
      'target_name': 'bitmap_format_java',
      'type': 'none',
      'variables': {
        'source_file': '../gfx/android/java_bitmap.h',
      },
      'includes': [ '../../build/android/java_cpp_enum.gypi' ],
    },
    {
      'target_name': 'page_transition_types_java',
      'type': 'none',
      'variables': {
        'source_file': '../base/page_transition_types.h',
      },
      'includes': [ '../../build/android/java_cpp_enum.gypi' ],
    },
    {
      'target_name': 'system_ui_resource_type_java',
      'type': 'none',
      'variables': {
        'source_file': 'resources/system_ui_resource_type.h',
      },
      'includes': [ '../../build/android/java_cpp_enum.gypi' ],
    },
    {
      'target_name': 'touch_device_types_java',
      'type': 'none',
      'variables': {
        'source_file': '../base/touch/touch_device.h',
      },
      'includes': [ '../../build/android/java_cpp_enum.gypi' ],
    },
    {
      'target_name': 'window_open_disposition_java',
      'type': 'none',
      'variables': {
        'source_file': '../base/window_open_disposition.h',
      },
      'includes': [ '../../build/android/java_cpp_enum.gypi' ],
    },
    {
      'target_name': 'text_input_type_java',
      'type': 'none',
      'variables': {
        'source_file': '../base/ime/text_input_type.h',
      },
      'includes': [ '../../build/android/java_cpp_enum.gypi' ],
    },
    {
      'target_name': 'ui_java',
      'type': 'none',
      'variables': {
        'java_in_dir': '../../ui/android/java',
        'has_java_resources': 1,
        'R_package': 'org.chromium.ui',
        'R_package_relpath': 'org/chromium/ui',
      },
      'dependencies': [
        '../../base/base.gyp:base_java',
        'android_resource_type_java',
        'bitmap_format_java',
        'page_transition_types_java',
        'system_ui_resource_type_java',
        'touch_device_types_java',
        'ui_strings_grd',
        'window_open_disposition_java',
        'text_input_type_java',
      ],
      'includes': [ '../../build/java.gypi' ],
    },
    {
      'target_name': 'ui_javatests',
      'type': 'none',
      'variables': {
        'java_in_dir': '../../ui/android/javatests',
      },
      'dependencies': [
        '../../base/base.gyp:base_java_test_support',
        'ui_java',
      ],
      'includes': [ '../../build/java.gypi' ],
    },
    {
      'target_name': 'ui_strings_grd',
       # The android_webview/Android.mk file depends on this target directly.
      'android_unmangled_name': 1,
      'type': 'none',
      'variables': {
        'grd_file': '../../ui/android/java/strings/android_ui_strings.grd',
      },
      'includes': [
        '../../build/java_strings_grd.gypi',
      ],
    },
    {
      # GN: //ui/android:ui_junit_tests
      'target_name': 'ui_junit_tests',
      'type': 'none',
      'dependencies': [
        'ui_java',
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_junit_test_support',
        '../../testing/android/junit/junit_test.gyp:junit_test_support',
      ],
      'variables': {
        'main_class': 'org.chromium.testing.local.JunitTestMain',
        'src_paths': [
          'junit/',
        ],
        'test_type': 'junit',
        'wrapper_script_name': 'helper/<(_target_name)',
      },
      'includes': [
        '../../build/android/test_runner.gypi',
        '../../build/host_jar.gypi',
      ],
    },
    {
      # GN version: //ui/android:ui_android_unittests
      'target_name': 'ui_android_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../cc/cc.gyp:cc',
        '../../cc/cc_tests.gyp:cc_test_support',
        '../../skia/skia.gyp:skia',
        '../../testing/android/native_test.gyp:native_test_native_code',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../base/ui_base.gyp:ui_base',
        '../gfx/gfx.gyp:gfx',
        '../resources/ui_resources.gyp:ui_test_pak',
        'ui_android',
      ],
      'sources': [
        'overscroll_refresh_unittest.cc',
        'resources/resource_manager_impl_unittest.cc',
        'run_all_unittests.cc',
      ],
    },
    {
      'target_name': 'ui_android_unittests_apk',
      'type': 'none',
      'dependencies': [
        'ui_android_unittests',
        'ui_java',
      ],
      'variables': {
        'test_suite_name': 'ui_android_unittests',
      },
      'includes': [ '../../build/apk_test.gypi' ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"',
      {
        'targets': [
          {
            'target_name': 'ui_android_unittests_apk_run',
            'type': 'none',
            'dependencies': [
              'ui_android_unittests_apk',
            ],
            'includes': [
              '../../build/isolate.gypi',
            ],
            'sources': [
              'ui_android_unittests_apk.isolate',
            ],
          },
        ]
      }
    ],
  ]
}
