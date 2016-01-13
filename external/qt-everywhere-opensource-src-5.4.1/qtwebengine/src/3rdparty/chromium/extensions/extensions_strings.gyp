# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //extensions:strings
      'target_name': 'extensions_strings',
      'type': 'none',
      'actions': [
        {
          'action_name': 'generate_extensions_strings',
          'variables': {
            'grit_grd_file': 'extensions_strings.grd',
            'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/extensions/strings',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/extensions/strings',
        ],
      },
      # This target generates extensions_strings.h so it must be built before
      # targets that depend on it.
      'hard_dependency': 1,
    },
  ],
}
