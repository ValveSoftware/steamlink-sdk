# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN: //third_party/ub-uiautomator:ub_uiautomator_java
      'target_name': 'ub_uiautomator_jar',
      'type': 'none',
      'variables': {
        'jar_path': 'lib/ub-uiautomator.jar',
      },
      'includes': [
        '../../build/java_prebuilt.gypi',
      ]
    },
  ],
}