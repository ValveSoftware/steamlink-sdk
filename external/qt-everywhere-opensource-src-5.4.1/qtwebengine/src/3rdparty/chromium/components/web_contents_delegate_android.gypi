# Copyright (c) 2012 The Chromium Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'web_contents_delegate_android',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content.gyp:content_browser',
            '../content/content.gyp:content_common',
            '../net/net.gyp:net',
            '../skia/skia.gyp:skia',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            'web_contents_delegate_android_jni_headers',
          ],
          'include_dirs': [
            '..',
            '../skia/config',
          ],
          'sources': [
            'web_contents_delegate_android/color_chooser_android.cc',
            'web_contents_delegate_android/color_chooser_android.h',
            'web_contents_delegate_android/component_jni_registrar.cc',
            'web_contents_delegate_android/component_jni_registrar.h',
            'web_contents_delegate_android/validation_message_bubble_android.cc',
            'web_contents_delegate_android/validation_message_bubble_android.h',
            'web_contents_delegate_android/web_contents_delegate_android.cc',
            'web_contents_delegate_android/web_contents_delegate_android.h',
          ],
        },
        {
          'target_name': 'web_contents_delegate_android_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content.gyp:content_java',
          ],
          'variables': {
            'java_in_dir': 'web_contents_delegate_android/android/java',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'web_contents_delegate_android_jni_headers',
          'type': 'none',
          'sources': [
            'web_contents_delegate_android/android/java/src/org/chromium/components/web_contents_delegate_android/ColorChooserAndroid.java',
            'web_contents_delegate_android/android/java/src/org/chromium/components/web_contents_delegate_android/ValidationMessageBubble.java',
            'web_contents_delegate_android/android/java/src/org/chromium/components/web_contents_delegate_android/WebContentsDelegateAndroid.java',
          ],
          'variables': {
            'jni_gen_package': 'web_contents_delegate_android',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
      ],
    }],
  ],
}
