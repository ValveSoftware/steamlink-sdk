# Copyright (c) 2012 The Chromium Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'navigation_interception',
      'type': 'static_library',
      'defines!': ['CONTENT_IMPLEMENTATION'],
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../content/content.gyp:content_common',
        '../net/net.gyp:net',
      ],
      'include_dirs': [
        '..',
        '../skia/config',
      ],
      'sources': [
        'navigation_interception/intercept_navigation_resource_throttle.cc',
        'navigation_interception/intercept_navigation_resource_throttle.h',
        'navigation_interception/navigation_params.h',
        'navigation_interception/navigation_params.cc',
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            'navigation_interception_jni_headers',
          ],
          'sources': [
            'navigation_interception/component_jni_registrar.cc',
            'navigation_interception/component_jni_registrar.h',
            'navigation_interception/intercept_navigation_delegate.cc',
            'navigation_interception/intercept_navigation_delegate.h',
            'navigation_interception/navigation_params_android.h',
            'navigation_interception/navigation_params_android.cc',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
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
          'target_name': 'navigation_interception_jni_headers',
          'type': 'none',
          'sources': [
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
