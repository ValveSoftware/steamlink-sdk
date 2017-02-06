# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # Leak Canary added to GN only.
      'target_name': 'leakcanary_java',
      'type': 'none',
      'variables': {
        'java_in_dir': 'src/leakcanary-android-no-op/src/main',
        'java_in_dir_suffix': '/java',
      },
      'includes': [ '../../build/java.gypi' ],
    },
  ],
}
