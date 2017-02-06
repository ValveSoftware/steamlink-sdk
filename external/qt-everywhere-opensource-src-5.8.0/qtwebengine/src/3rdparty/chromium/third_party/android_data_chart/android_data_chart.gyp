# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'android_data_chart_java',
      'type': 'none',
      'variables': {
        'java_in_dir': 'java',
        'has_java_resources': 1,
        'R_package': 'org.chromium.third_party.android',
        'R_package_relpath': 'org/chromium/third_party/android',
      },
      'includes': [ '../../build/java.gypi' ],
    },
  ],
}
