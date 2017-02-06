# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/app_list/resources
      'target_name': 'app_list_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/app_list/resources',
      },
      'actions': [{
        'action_name': 'app_list_resources',
        'variables': {
          'grit_grd_file': 'app_list_resources.grd',
        },
        'includes': [ '../../../build/grit_action.gypi' ],
      }],
      'includes': [ '../../../build/grit_target.gypi' ],
    },
  ],
}
