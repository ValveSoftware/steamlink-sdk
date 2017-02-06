# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    # GN: //android_webview/test:android_webview_apk
    {
      'target_name': 'android_webview_apk',
      'type': 'none',
      'dependencies': [
        'libstandalonelibwebviewchromium',
        'android_webview_java',
        'android_webview_pak',
        'libdrawgl',
        '../base/base.gyp:base_java_test_support',
        '../components/components.gyp:policy_java_test_support'
      ],
      'variables': {
        'apk_name': 'AndroidWebView',
        'java_in_dir': 'test/shell',
        'native_lib_target': 'libstandalonelibwebviewchromium',
        'native_lib_version_name': '<(version_full)',
        'resource_dir': 'test/shell/res',
        'extensions_to_not_compress': '.lpak,.pak,.dat,.bin',
        'asset_location': '<(PRODUCT_DIR)/android_webview_apk/assets',
        'extra_native_libs': ['<(SHARED_LIB_DIR)/libdrawgl.>(android_product_extension)'],
        'snapshot_copy_files': '<(snapshot_copy_files)',
        'additional_input_paths': [
          '<(asset_location)/webviewchromium.pak',
          '<(asset_location)/asset_file.html',
          '<(asset_location)/cookie_test.html',
          '<(asset_location)/asset_icon.png',
          '<(asset_location)/full_screen_video.js',
          '<(asset_location)/full_screen_video_test.html',
          '<(asset_location)/full_screen_video_inside_div_test.html',
          '<(asset_location)/multiple_videos_test.html',
          '<(asset_location)/platform-media-codec-test.html',
          '<(asset_location)/video.3gp',
          '<(asset_location)/video.webm',
          '<(asset_location)/visual_state_during_fullscreen_test.html',
          '<(asset_location)/visual_state_waits_for_js_test.html',
          '<(asset_location)/visual_state_waits_for_js_detached_test.html',
          '<(asset_location)/visual_state_on_page_commit_visible_test.html',
          '<@(snapshot_additional_input_paths)',
        ],
        'conditions': [
          ['icu_use_data_file_flag==1', {
            'additional_input_paths': [
              '<(asset_location)/icudtl.dat',
            ],
          }],
        ],
        'includes': [ 'snapshot_copying.gypi' ],
      },
      'copies': [
        {
          'destination': '<(asset_location)',
          'files': [
            '<(PRODUCT_DIR)/android_webview_assets/webviewchromium.pak',
            '<(java_in_dir)/assets/asset_file.html',
            '<(java_in_dir)/assets/asset_icon.png',
            '<(java_in_dir)/assets/cookie_test.html',
            '<(java_in_dir)/assets/full_screen_video.js',
            '<(java_in_dir)/assets/full_screen_video_test.html',
            '<(java_in_dir)/assets/full_screen_video_inside_div_test.html',
            '<(java_in_dir)/assets/multiple_videos_test.html',
            '<(java_in_dir)/assets/platform-media-codec-test.html',
            '<(java_in_dir)/assets/video.3gp',
            '<(java_in_dir)/assets/video.webm',
            '<(java_in_dir)/assets/visual_state_during_fullscreen_test.html',
            '<(java_in_dir)/assets/visual_state_waits_for_js_test.html',
            '<(java_in_dir)/assets/visual_state_waits_for_js_detached_test.html',
            '<(java_in_dir)/assets/visual_state_on_page_commit_visible_test.html',
            '<@(snapshot_copy_files)',
          ],
          'conditions': [
            ['icu_use_data_file_flag==1', {
              'files': [
                '<(PRODUCT_DIR)/icudtl.dat',
              ],
            }],
          ],
        },
      ],
      'includes': [
        '../build/java_apk.gypi',
        '../build/util/version.gypi',
      ],
    },
    {
      # android_webview_apk creates a .jar as a side effect. Any java
      # targets that need that .jar in their classpath should depend on this
      # target. For more details see the content_shell_apk_java target.
      'target_name': 'android_webview_apk_java',
      'type': 'none',
      'dependencies': [
        'android_webview_apk',
      ],
      'includes': [ '../build/apk_fake_jar.gypi' ],
    },
    # GN: //android_webview/test:android_webview_test_apk
    {
      'target_name': 'android_webview_test_apk',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base_java_test_support',
        '../components/components.gyp:policy_java_test_support',
        '../content/content_shell_and_tests.gyp:content_java_test_support',
        '../net/net.gyp:net_java_test_support',
        '../testing/android/on_device_instrumentation.gyp:broker_java',
        '../testing/android/on_device_instrumentation.gyp:require_driver_apk',
        'android_webview_apk_java',
      ],
      'variables': {
        'apk_name': 'AndroidWebViewTest',
        'java_in_dir': '../android_webview/javatests',
        'is_test_apk': 1,
        'test_type': 'instrumentation',
        'isolate_file': 'android_webview_test_data.isolate',
      },
      'includes': [
        '../build/java_apk.gypi',
        '../build/android/test_runner.gypi',
      ],
    },
    # GN: //android_webview/test:android_webview_unittests
    {
      'target_name': 'android_webview_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:test_support_base',
        '../content/content_shell_and_tests.gyp:test_support_content',
        '../net/net.gyp:net_test_support',
        '../testing/android/native_test.gyp:native_test_native_code',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../ui/base/ui_base.gyp:ui_base_jni_headers',
        '../ui/gl/gl.gyp:gl',
        '../ui/gl/init/gl_init.gyp:gl_init',
        '../ui/gl/gl.gyp:gl_test_support',
        'android_webview_common',
        'android_webview_unittests_jni',
      ],
      'include_dirs': [
        '..',
        '../skia/config',
        '<(SHARED_INTERMEDIATE_DIR)/android_webview_unittests',
      ],
      'sources': [
        'browser/aw_static_cookie_policy_unittest.cc',
        'browser/aw_form_database_service_unittest.cc',
        'browser/browser_view_renderer_unittest.cc',
        'browser/net/android_stream_reader_url_request_job_unittest.cc',
        'browser/net/aw_cookie_store_wrapper_unittest.cc',
        'browser/net/input_stream_reader_unittest.cc',
        'browser/test/fake_window.cc',
        'browser/test/fake_window.h',
        'browser/test/rendering_test.cc',
        'browser/test/rendering_test.h',
        'lib/main/webview_tests.cc',
        'native/aw_contents_client_bridge_unittest.cc',
        'native/aw_media_url_interceptor_unittest.cc',
        'native/input_stream_unittest.cc',
        'native/permission/media_access_permission_request_unittest.cc',
        'native/permission/permission_request_handler_unittest.cc',
        'native/state_serializer_unittest.cc',
      ],
    },
    # GN: //android_webview/test:android_webview_unittest_java
    {
      'target_name': 'android_webview_unittest_java',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base_java_test_support',
        '../content/content_shell_and_tests.gyp:content_java_test_support',
        'android_webview_java',
      ],
      'variables': {
        'java_in_dir': '../android_webview/unittestjava',
      },
      'includes': [ '../build/java.gypi' ],
    },
    # GN: //android_webview/test:android_webview_unittests_jni
    {
      'target_name': 'android_webview_unittests_jni',
      'type': 'none',
      'sources': [
          '../android_webview/unittestjava/src/org/chromium/android_webview/unittest/InputStreamUnittest.java',
          '../android_webview/unittestjava/src/org/chromium/android_webview/unittest/MockAwContentsClientBridge.java',
      ],
      'variables': {
        'jni_gen_package': 'android_webview_unittests',
      },
      'includes': [ '../build/jni_generator.gypi' ],
    },
    # GN: //android_webview/test:android_webview_unittests_apk
    {
      'target_name': 'android_webview_unittests_apk',
      'type': 'none',
      'dependencies': [
        'android_webview_unittest_java',
        'android_webview_unittests',
      ],
      'variables': {
        'test_suite_name': 'android_webview_unittests',
        'additional_input_paths': [
          '<(PRODUCT_DIR)/android_webview_unittests_apk/assets/asset_file.ogg',
        ],
      },
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/android_webview_unittests_apk/assets',
          'files': [
            'test/unittest/assets/asset_file.ogg',
          ],
        },
      ],
      'includes': [ '../build/apk_test.gypi' ],
    },
    # GN: //android_webview/test:libdrawgl
    {
      'target_name': 'libdrawgl',
      'type': 'shared_library',
      # Do not depend on any other component here, since this target
      # builds a separate shared library!
      'include_dirs': [
        '..',
      ],
      'variables': {
        # This library uses native JNI exports; tell gyp so that the required
        # symbols will be kept.
        'use_native_jni_exports': 1,
      },
      'sources': [
          '../android_webview/test/shell/src/draw_gl/draw_gl.cc',
      ],
    },
    # GN: //android_webview/test:libstandalonelibwebviewchromium
    {
      'target_name': 'libstandalonelibwebviewchromium',
      'includes': [
        'libwebviewchromium.gypi',
      ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"',
      {
        'targets': [
          {
            'target_name': 'android_webview_test_apk_run',
            'type': 'none',
            'dependencies': [
              'android_webview_test_apk',
            ],
            'includes': [
              '../build/isolate.gypi',
            ],
            'sources': [
              'android_webview_test_apk.isolate',
            ],
          },
          {
            'target_name': 'android_webview_unittests_apk_run',
            'type': 'none',
            'dependencies': [
              'android_webview_unittests_apk',
            ],
            'includes': [
              '../build/isolate.gypi',
            ],
            'sources': [
              'android_webview_unittests_apk.isolate',
            ],
          },
        ]
      }
    ],
  ]
}
