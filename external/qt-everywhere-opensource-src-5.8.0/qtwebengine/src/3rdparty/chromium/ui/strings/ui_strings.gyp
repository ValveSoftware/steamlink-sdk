# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'ui_strings',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/strings',
      },
      'actions': [
        {
          'action_name': 'generate_ui_strings',
          'variables': {
            'grit_grd_file': 'ui_strings.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
        {
          'action_name': 'generate_app_locale_settings',
          'variables': {
            'grit_grd_file': 'app_locale_settings.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
    },
  ],
}
