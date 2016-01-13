# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'extensions_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/extensions',
      },
      'actions': [
        {
          'action_name': 'extensions_resources',
          'variables': {
            'grit_grd_file': 'extensions_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'extensions_renderer_resources',
          'variables': {
            'grit_grd_file': 'renderer/resources/extensions_renderer_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/extensions',
        ]
      },
      'hard_dependency': 1,
    }
  ]
}
