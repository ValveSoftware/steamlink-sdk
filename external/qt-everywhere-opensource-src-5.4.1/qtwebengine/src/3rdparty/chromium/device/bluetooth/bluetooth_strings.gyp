# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'device_bluetooth_strings',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/device/bluetooth',
      },
      'actions': [
        {
          'action_name': 'device_bluetooth_strings',
          'variables': {
            'grit_grd_file': 'bluetooth_strings.grd',
            'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/device/bluetooth/strings',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/device/bluetooth/strings',
        ],
      },
    },
  ],
}
