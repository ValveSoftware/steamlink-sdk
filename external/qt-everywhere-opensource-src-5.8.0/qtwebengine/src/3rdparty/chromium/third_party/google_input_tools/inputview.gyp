# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'inputview',
      'type': 'none',
      'includes': [ 'inputview.gypi' ],
      'actions': [
        {
          'action_name': 'inputview',
          'inputs': [
            'builder.py',
            '<@(inputview_sources)',
            'inputview.gypi',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/ui/keyboard/resources/inputview.js',
          ],
          'action': [
            'python',
            'builder.py',
            '--target',
            '<@(_outputs)',
            '--json_file',
            'inputview.gypi',
            '--json_sources',
            'variables.inputview_sources'
          ],
          'message': 'Generating <@(_outputs)'
        },
      ],
    },
  ],
}