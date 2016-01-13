# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'ui_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources',
      },
      'actions': [
        {
          'action_name': 'ui_resources',
          'variables': {
            'grit_grd_file': 'ui_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
        {
          'action_name': 'webui_resources',
          'variables': {
            'grit_grd_file': '../webui/resources/webui_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
        {
          'action_name': 'ui_unscaled_resources',
          'variables': {
            'grit_grd_file': 'ui_unscaled_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
    },
    {
      # This creates a pak file that contains the resources in src/ui.
      # This pak file can be used by tests.
      'target_name': 'ui_test_pak',
      'type': 'none',
      'dependencies': [
        '../strings/ui_strings.gyp:ui_strings',
        'ui_resources',
      ],
      'actions': [
        {
          'action_name': 'repack_ui_test_pack',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/ui/app_locale_settings/app_locale_settings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/webui_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/ui_strings/ui_strings_en-US.pak',
            ],
            'pak_output': '<(PRODUCT_DIR)/ui_test.pak',
          },
          'includes': [ '../../build/repack_action.gypi' ],
        },
      ],
      'conditions': [
        ['OS != "mac"', {
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/ui',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources_100_percent.pak',
              ],
            },
          ],
        }],
        ['OS == "ios"', {
          'actions': [
            {
              'action_name': 'copy_ui_test_pak',
              'message': 'Copying ui_test.pak into locale.pak',
              'inputs': [
                '<(PRODUCT_DIR)/ui_test.pak',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/ui/en.lproj/locale.pak',
              ],
              'action': [
                'python',
                '../../build/cp.py',
                '<@(_inputs)',
                '<@(_outputs)'
              ],
            },
          ],
        }],
        ['OS != "mac" and OS !="ios"', {
          'actions': [
            {
              'action_name': 'copy_ui_test_pak',
              'message': 'Copying ui_test.pak into en-US.pak',
              'inputs': [
                '<(PRODUCT_DIR)/ui_test.pak',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/ui/en-US.pak',
              ],
              'action': [
                'python',
                '../../build/cp.py',
                '<@(_inputs)',
                '<@(_outputs)'
              ],
            },
          ],
        }],
      ],
    },
  ],
}
