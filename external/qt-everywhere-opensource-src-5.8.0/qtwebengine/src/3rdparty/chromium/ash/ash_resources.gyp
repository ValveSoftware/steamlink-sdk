# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //ash/resources
      'target_name': 'ash_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ash/resources',
      },
      'actions': [
        {
          'action_name': 'ash_resources',
          'variables': {
            'grit_grd_file': 'resources/ash_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      # Creates a 100% pak file containing resources for ash_unittests, etc.
      # TODO(msw): Use ui_test.pak instead of its pieces? (no 200% support?)
      # TODO(msw): Add copy steps and mac handling like repack_ui_test_pack?
      # GN version: //ash/resources:ash_test_resources_100_percent
      'target_name': 'ash_test_resources_100_percent',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/ui/resources/ui_resources.gyp:ui_resources',
        'ash_resources',
      ],
      'actions': [
        {
          'action_name': 'repack_ash_test_resources',
          'variables': {
            'pak_output': '<(PRODUCT_DIR)/ash_test_resources_100_percent.pak',
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/ash/resources/ash_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/webui_resources.pak',
            ],
            'conditions': [
              ['chromeos==1', {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/ui/chromeos/resources/ui_chromeos_resources_100_percent.pak',
                ],
              }],
              ['toolkit_views==1', {
                'pak_inputs': [
                  # TODO(msw): This seems bad, but follows repack_ui_test_pack's example.
                  '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/ui/app_list/resources/app_list_resources_100_percent.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/ui/views/resources/views_resources_100_percent.pak',
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
            '<(DEPTH)/ui/chromeos/ui_chromeos.gyp:ui_chromeos_resources',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '<(DEPTH)/ui/views/resources/views_resources.gyp:views_resources',
          ],
        }],
      ],
    },
    {
      # Creates a 200% pak file containing resources for ash_unittests, etc.
      # TODO(msw): Use ui_test.pak instead of its pieces? (no 200% support?)
      # TODO(msw): Add copy steps and mac handling like repack_ui_test_pack?
      # GN version: //ash/resources:ash_test_resources_200_percent
      'target_name': 'ash_test_resources_200_percent',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/ui/resources/ui_resources.gyp:ui_resources',
        'ash_resources',
      ],
      'actions': [
        {
          'action_name': 'repack_ash_test_resources',
          'variables': {
            'pak_output': '<(PRODUCT_DIR)/ash_test_resources_200_percent.pak',
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/ash/resources/ash_resources_200_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_200_percent.pak',
            ],
            'conditions': [
              ['chromeos==1', {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/ui/chromeos/resources/ui_chromeos_resources_200_percent.pak',
                ],
              }],
              ['toolkit_views==1', {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/ui/app_list/resources/app_list_resources_200_percent.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/ui/views/resources/views_resources_200_percent.pak',
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
            '<(DEPTH)/ui/chromeos/ui_chromeos.gyp:ui_chromeos_resources',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '<(DEPTH)/ui/views/resources/views_resources.gyp:views_resources',
          ],
        }],
      ],
    },
  ],
}
