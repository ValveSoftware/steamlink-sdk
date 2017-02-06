# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'android_media_java',
      'type': 'none',
          'dependencies': [
            '../android_tools/android_tools.gyp:android_support_v13_javalib',
          ],
      'variables': {
        'java_in_dir': 'java',
        'has_java_resources': 1,
        'R_package': 'org.chromium.third_party.android.media',
        'R_package_relpath': 'org/chromium/third_party/android/media',
      },
      'includes': [ '../../build/java.gypi' ],
    },
  ],
}
