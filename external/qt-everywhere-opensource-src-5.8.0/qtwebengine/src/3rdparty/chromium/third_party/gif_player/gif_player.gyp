# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'gif_player_java',
      'type': 'none',
      'variables': {
        'java_in_dir': './',
        'run_findbugs': 0,
      },
      'includes': [ '../../build/java.gypi' ],
    },
  ],
}
