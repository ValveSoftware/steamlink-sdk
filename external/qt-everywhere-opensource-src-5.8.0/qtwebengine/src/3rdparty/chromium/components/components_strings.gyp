# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'components_strings',
      'type': 'none',
      'hard_dependency': 1,
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/components/strings',
      },
      'actions': [
        {
          # GN version: //components/strings:components_strings
          'action_name': 'generate_components_strings',
          'variables': {
            'grit_grd_file': 'components_strings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          # GN version: //components/strings:components_chromium_strings
          'action_name': 'generate_components_chromium_strings',
          'variables': {
            'grit_grd_file': 'components_chromium_strings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          # GN version: //components/strings:components_google_chrome_strings
          'action_name': 'generate_components_google_chrome_strings',
          'variables': {
            'grit_grd_file': 'components_google_chrome_strings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          # GN version: //components/strings:components_locale_settings
          'action_name': 'generate_components_locale_settings',
          'variables': {
            'grit_grd_file': 'components_locale_settings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
  ],
}
