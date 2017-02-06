# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //ui/resources
      'target_name': 'ui_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/resources',
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
      #
      # GN version: //ui/resources:ui_test_pak
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
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/webui_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/strings/app_locale_settings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/strings/ui_strings_en-US.pak',
            ],
            'pak_output': '<(PRODUCT_DIR)/ui_test.pak',
            'conditions': [
              ['chromeos==1', {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/ui/chromeos/resources/ui_chromeos_resources_100_percent.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/ui/chromeos/strings/ui_chromeos_strings_en-US.pak',
                ],
              }],
              ['toolkit_views==1', {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/ui/views/resources/views_resources_100_percent.pak',
                ],
              }],
              ['use_aura==1 and toolkit_views==1', {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/ui/app_list/resources/app_list_resources_100_percent.pak',
                ],
              }],
            ],
          },
          'includes': [ '../../build/repack_action.gypi' ],
        },
      ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '../chromeos/ui_chromeos.gyp:ui_chromeos_strings',
            '../chromeos/ui_chromeos.gyp:ui_chromeos_resources',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../views/resources/views_resources.gyp:views_resources',
          ],
        }],
        ['OS != "mac"', {
          'copies': [
            {
              # GN version //ui/resources:copy_ui_resources_100_percent
              'destination': '<(PRODUCT_DIR)/ui',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_100_percent.pak',
              ],
            },
          ],
        }],
        ['OS == "ios" or OS == "mac"', {
          'actions': [
            {
              # GN version: //ui/resources:repack_ui_test_mac_locale_pack
              # Repack just the strings for the framework locales on Mac and
              # iOS. This emulates repack_locales.py, but just for en-US. Note
              # ui_test.pak is not simply copied, because it causes leaks from
              # allocations within system libraries when trying to load non-
              # string resources. http://crbug.com/413034.
              'action_name': 'repack_ui_test_mac_locale_pack',
              'variables': {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/ui/strings/app_locale_settings_en-US.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/ui/strings/ui_strings_en-US.pak',
                ],
                'pak_output': '<(PRODUCT_DIR)/ui/en.lproj/locale.pak',
              },
              'includes': [ '../../build/repack_action.gypi' ],
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
