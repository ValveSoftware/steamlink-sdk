# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'grit_base_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui',
  },
  'targets': [
    {
      'target_name': 'ui_strings',
      'type': 'none',
      'actions': [
        {
          'action_name': 'ui_strings',
          'variables': {
            'grit_grd_file': 'ui_strings.grd',
            'grit_out_dir': '<(grit_base_out_dir)/ui_strings',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
        {
          'action_name': 'app_locale_settings',
          'variables': {
            'grit_grd_file': 'app_locale_settings.grd',
            'grit_out_dir': '<(grit_base_out_dir)/app_locale_settings',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(grit_base_out_dir)/app_locale_settings',
          '<(grit_base_out_dir)/ui_strings',
        ],
      },
    },
  ],
}
