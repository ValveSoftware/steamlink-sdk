# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [{
    # GN version: //ui/platform_window/android:android_window
    'target_name': 'android_window',
    'type': '<(component)',
    'dependencies': [
      '../../../base/base.gyp:base',
      '../../../skia/skia.gyp:skia',
      '../../events/events.gyp:events',
      '../../events/events.gyp:events_base',
      '../../gfx/gfx.gyp:gfx',
      '../../gfx/gfx.gyp:gfx_geometry',
      '../platform_window.gyp:platform_window',
      'android_window_jni_headers',
      'android_window_java',
    ],
    'defines': [ 'ANDROID_WINDOW_IMPLEMENTATION' ],
    'sources': [
      'android_window_export.h',
      'platform_ime_controller_android.cc',
      'platform_ime_controller_android.h',
      'platform_window_android.cc',
      'platform_window_android.h',
    ],
  },
  {
    # GN version: //ui/platform_window/android:jni_headers
    'target_name': 'android_window_jni_headers',
    'type': 'none',
    'sources': [
      '../android/java/src/org/chromium/ui/PlatformImeControllerAndroid.java',
      '../android/java/src/org/chromium/ui/PlatformWindowAndroid.java',
    ],
    'variables': {
      'jni_gen_package': 'android_window',
    },
    'includes': [ '../../../build/jni_generator.gypi' ],
  },
  {
    # GN version: //ui/platform_window/android:platform_window_java
    'target_name': 'android_window_java',
    'type': 'none',
    'variables': {
      'java_in_dir': '../../../ui/platform_window/android/java',
    },
    'dependencies': [
      '../../../base/base.gyp:base_java',
    ],
    'includes': [ '../../../build/java.gypi' ],
  }],
}
