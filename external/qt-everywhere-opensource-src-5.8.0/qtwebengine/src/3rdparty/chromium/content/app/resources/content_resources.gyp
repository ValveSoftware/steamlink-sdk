# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //content/app/resources
      'target_name': 'content_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/content/app/resources/',
      },
      'actions': [
        {
          'action_name': 'generate_content_resources',
          'variables': {
            'grit_grd_file': 'content_resources.grd',
          },
          'includes': [ '../../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../../build/grit_target.gypi' ],
    },
  ]
}
