# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'haha_java',
      'type': 'none',
      'variables': {
        'jar_path': 'haha-2.0.2.jar',
      },
      'includes': [ '../../build/java_prebuilt.gypi' ],
    },
  ],
}
