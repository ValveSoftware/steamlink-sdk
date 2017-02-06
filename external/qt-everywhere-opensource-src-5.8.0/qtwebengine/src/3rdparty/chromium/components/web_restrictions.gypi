# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          # GN: //components/web_restrictions:web_restrictions_java
          'target_name': 'web_restrictions_java',
          'type': 'none',
          'variables': {
            'java_in_dir': 'web_restrictions/browser/java',
          },
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'includes': [ '../build/java.gypi' ],
        },
        {
          # GN: //components/web_restrictions:web_restrictions_jni_headers
          'target_name': 'web_restrictions_jni_headers',
          'type': 'none',
          'sources': [
            'web_restrictions/browser/java/src/org/chromium/components/webrestrictions/WebRestrictionsClient.java',
          ],
          'variables': {
            'jni_gen_package': 'components/web_restrictions',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          # GN: //components/web_restrictions:browser
          'target_name': 'web_restrictions_browser',
          'type': 'static_library',
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/content/content.gyp:content_browser',
            'web_restrictions_jni_headers',
          ],
          'sources': [
            "web_restrictions/browser/web_restrictions_client.cc",
            "web_restrictions/browser/web_restrictions_client.h",
            "web_restrictions/browser/web_restrictions_resource_throttle.cc",
            "web_restrictions/browser/web_restrictions_resource_throttle.h",
          ],
        },
        {
          # GN: //components/web_restrictions:renderer
          'target_name': 'web_restrictions_renderer',
          'type': 'static_library',
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/content/content.gyp:content_browser',
            'web_restrictions_jni_headers',
          ],
          'sources': [
            "web_restrictions/renderer/web_restrictions_gin_wrapper.cc",
            "web_restrictions/renderer/web_restrictions_gin_wrapper.h",
          ],
        },
        {
          'target_name': 'web_restrictions_test_support_jni_headers',
          'type': 'none',
          'sources': [
            'web_restrictions/browser/javatest/src/org/chromium/components/webrestrictions/MockWebRestrictionsClient.java',
          ],
          'variables': {
            'jni_gen_package': 'components/web_restrictions',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'web_restrictions_test_support_java',
          'type': 'none',
          'dependencies': [
           'components.gyp:web_restrictions_java',
             '../base/base.gyp:base_java',
          ],
          'variables': {
            'java_in_dir': [
               'web_restrictions/browser/javatest/'
            ],
          },
          'includes': [ '../build/java.gypi' ],
         },
         {
          'target_name': 'web_restrictions_test_support',
          'type': 'static_library',
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
            'web_restrictions_test_support_jni_headers',
          ],
          'sources': [
            'web_restrictions/browser/mock_web_restrictions_client.cc',
            'web_restrictions/browser/mock_web_restrictions_client.h',
          ],
         },
         {
           'target_name': 'components_web_restrictions_junit_tests',
           'type': 'none',
           'dependencies': [
             '../testing/android/junit/junit_test.gyp:junit_test_support',
           ],
           'variables': {
             'main_class': 'org.chromium.testing.local.JunitTestMain',
             'src_paths': [
               '../testing/android/junit/DummyTest.java',
             ],
             'wrapper_script_name': 'helper/<(_target_name)',
           },
           'includes': [ '../build/host_jar.gypi' ],
         },
      ],
  }]]
}
