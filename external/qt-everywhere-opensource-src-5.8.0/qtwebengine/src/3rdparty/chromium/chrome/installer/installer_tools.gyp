# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          # A target that is outdated if any of the mini_installer test sources
          # are modified.
          'target_name': 'test_installer_sentinel',
          'type': 'none',
          'includes': [
            '../test/mini_installer/test_installer.gypi',
          ],
          'actions': [
            {
              'action_name': 'touch_sentinel',
              'variables': {
                'touch_sentinel_py': '../tools/build/win/touch_sentinel.py',
              },
              'inputs': [
                '<@(test_installer_sources)',  # from test_installer.gypi
                '<(touch_sentinel_py)',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/chrome/installer/test_installer_sentinel',
              ],
              'action': ['python', '<(touch_sentinel_py)', '<@(_outputs)'],
            },
          ],
        },
      ],
    }],
  ],
}
