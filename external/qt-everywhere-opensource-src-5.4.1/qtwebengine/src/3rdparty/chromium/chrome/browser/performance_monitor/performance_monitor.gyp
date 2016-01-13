# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'performance_monitor',
      'type': 'static_library',
      'sources': [
        '<@(schema_files)',
      ],
      'includes': [
        '../../../build/json_schema_compile.gypi',
      ],
      'variables': {
        'chromium_code': 1,
        'schema_files': [
          'events.json',
        ],
        'cc_dir': 'chrome/browser/performance_monitor',
        'root_namespace': 'performance_monitor',
      },
    },
  ],
}
