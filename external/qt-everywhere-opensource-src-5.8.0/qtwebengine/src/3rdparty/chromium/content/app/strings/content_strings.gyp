# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //content/app/strings
      'target_name': 'content_strings',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/content/app/strings',
      },
      'actions': [
        {
          'action_name': 'generate_content_strings',
          'variables': {
            'grit_grd_file': 'content_strings.grd',
          },
          'includes': [ '../../../build/grit_action.gypi' ],
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
    },
  ]
}
