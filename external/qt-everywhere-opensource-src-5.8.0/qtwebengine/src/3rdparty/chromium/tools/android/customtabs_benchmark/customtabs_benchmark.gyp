# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'customtabs_benchmark_apk',
      'type': 'none',
      'variables': {
        'java_in_dir': '../../../tools/android/customtabs_benchmark/java/',
        'apk_name': 'CustomTabsBenchmark',
      },
      'dependencies': [
        '../../../third_party/custom_tabs_client/custom_tabs_client.gyp:custom_tabs_support_lib'
      ],
      'includes': [ '../../../build/java_apk.gypi' ],
  }]
}

