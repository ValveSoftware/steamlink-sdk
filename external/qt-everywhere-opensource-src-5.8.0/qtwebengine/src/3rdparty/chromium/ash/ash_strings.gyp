# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //ash/strings
      'target_name': 'ash_strings',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ash/strings',
      },
      'actions': [
        {
          'action_name': 'generate_ash_strings',
          'variables': {
            'grit_grd_file': 'ash_strings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      # Creates a pak file containing strings for ash_unittests, etc.
      # GN version: //ash/strings:ash_test_strings
      'target_name': 'ash_test_strings',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/ui/strings/ui_strings.gyp:ui_strings',
        'ash_strings.gyp:ash_strings',
      ],
      'actions': [
        {
          'action_name': 'repack_ash_test_strings',
          'variables': {
            'pak_output': '<(PRODUCT_DIR)/ash_test_strings.pak',
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/ash/strings/ash_strings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/strings/app_locale_settings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/strings/ui_strings_en-US.pak',
            ],
            'conditions': [
              ['chromeos==1', {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/device/bluetooth/strings/bluetooth_strings_en-US.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/ui/chromeos/strings/ui_chromeos_strings_en-US.pak',
                ],
              }],
            ],
          },
          'includes': [ '../build/repack_action.gypi' ],
        },
      ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '<(DEPTH)/device/bluetooth/bluetooth_strings.gyp:bluetooth_strings',
            '<(DEPTH)/ui/chromeos/ui_chromeos.gyp:ui_chromeos_strings',
          ],
        }],
      ],
    },
  ],
}
