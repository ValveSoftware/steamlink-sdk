# Copyright (c) 2012 The Chromium Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/navigation_interception
      'target_name': 'navigation_interception',
      'type': 'static_library',
      'defines!': ['CONTENT_IMPLEMENTATION'],
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../content/content.gyp:content_common',
        '../net/net.gyp:net',
        '../ui/base/ui_base.gyp:ui_base',
      ],
      'include_dirs': [
        '..',
        '../skia/config',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'navigation_interception/intercept_navigation_throttle.cc',
        'navigation_interception/intercept_navigation_throttle.h',
        'navigation_interception/navigation_params.cc',
        'navigation_interception/navigation_params.h',
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            'navigation_interception_jni_headers',
          ],
          'sources': [
            # Note: sources list duplicated in GN build.
            'navigation_interception/component_jni_registrar.cc',
            'navigation_interception/component_jni_registrar.h',
            'navigation_interception/intercept_navigation_delegate.cc',
            'navigation_interception/intercept_navigation_delegate.h',
            'navigation_interception/navigation_params_android.cc',
            'navigation_interception/navigation_params_android.h',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          # GN version: //components/navigation_intercaption/android:navigation_interception_java
          'target_name': 'navigation_interception_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'variables': {
            'java_in_dir': 'navigation_interception/android/java',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          # GN version: //components/navigation_intercaption:jni_headers
          'target_name': 'navigation_interception_jni_headers',
          'type': 'none',
          'sources': [
            # Note: sources list duplicated in GN build.
            'navigation_interception/android/java/src/org/chromium/components/navigation_interception/InterceptNavigationDelegate.java',
            'navigation_interception/android/java/src/org/chromium/components/navigation_interception/NavigationParams.java',
          ],
          'variables': {
            'jni_gen_package': 'navigation_interception',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
      ],
    }],
  ],
}
