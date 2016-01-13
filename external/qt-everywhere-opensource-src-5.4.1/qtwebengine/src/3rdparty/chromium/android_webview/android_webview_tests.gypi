# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'android_webview_apk',
      'type': 'none',
      'dependencies': [
        'libwebviewchromium',
        'android_webview_java',
        'android_webview_pak',
      ],
      'variables': {
        'apk_name': 'AndroidWebView',
        'java_in_dir': 'test/shell',
        'native_lib_target': 'libstandalonelibwebviewchromium',
        'resource_dir': 'test/shell/res',
        'additional_input_paths': [
          '<(PRODUCT_DIR)/android_webview_apk/assets/webviewchromium.pak',
          '<(PRODUCT_DIR)/android_webview_apk/assets/asset_file.html',
          '<(PRODUCT_DIR)/android_webview_apk/assets/cookie_test.html',
          '<(PRODUCT_DIR)/android_webview_apk/assets/asset_icon.png',
          '<(PRODUCT_DIR)/android_webview_apk/assets/full_screen_video_test.html',
        ],
        'conditions': [
          ['icu_use_data_file_flag==1', {
            'additional_input_paths': [
              '<(PRODUCT_DIR)/icudtl.dat',
            ],
          }],
        ],
      },
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/android_webview_apk/assets',
          'files': [
            '<(java_in_dir)/assets/asset_file.html',
            '<(java_in_dir)/assets/asset_icon.png',
            '<(java_in_dir)/assets/cookie_test.html',
            '<(java_in_dir)/assets/full_screen_video_test.html',
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
      'includes': [ '../build/java_apk.gypi' ],
    },
    {
      # android_webview_apk creates a .jar as a side effect. Any java
      # targets that need that .jar in their classpath should depend on this
      # target. For more details see the chrome_shell_apk_java target.
      'target_name': 'android_webview_apk_java',
      'type': 'none',
      'dependencies': [
        'android_webview_apk',
      ],
      'includes': [ '../build/apk_fake_jar.gypi' ],
    },
    {
      'target_name': 'android_webview_test_apk',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base_java_test_support',
        '../content/content_shell_and_tests.gyp:content_java_test_support',
        '../net/net.gyp:net_java_test_support',
        'android_webview_apk_java',
      ],
      'variables': {
        'apk_name': 'AndroidWebViewTest',
        'java_in_dir': '../android_webview/javatests',
        'is_test_apk': 1,
      },
      'includes': [ '../build/java_apk.gypi' ],
    },
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
        'browser/global_tile_manager_unittest.cc',
        'browser/net/android_stream_reader_url_request_job_unittest.cc',
        'browser/net/input_stream_reader_unittest.cc',
        'lib/main/webview_tests.cc',
        'native/aw_contents_client_bridge_unittest.cc',
        'native/aw_media_url_interceptor_unittest.cc',
        'native/input_stream_unittest.cc',
        'native/permission/media_access_permission_request_unittest.cc',
        'native/permission/permission_request_handler_unittest.cc',
        'native/state_serializer_unittest.cc',
      ],
    },
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
  ],
}
