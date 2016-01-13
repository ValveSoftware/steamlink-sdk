# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'window_open_disposition_java',
      'type': 'none',
      'sources': [
        'java/WindowOpenDisposition.template',
      ],
      'variables': {
        'package_name': 'org/chromium/ui',
        'template_deps': ['../base/window_open_disposition_list.h'],
      },
      'includes': [ '../../build/android/java_cpp_template.gypi' ],
    },
    {
      'target_name': 'bitmap_format_java',
      'type': 'none',
      'sources': [
        'java/BitmapFormat.template',
      ],
      'variables': {
        'package_name': 'org/chromium/ui/gfx',
        'template_deps': ['../gfx/android/bitmap_config_list.h'],
      },
      'includes': [ '../../build/android/java_cpp_template.gypi' ],
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
        'bitmap_format_java',
        'ui_strings_grd',
        'window_open_disposition_java',
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
  ],
}
