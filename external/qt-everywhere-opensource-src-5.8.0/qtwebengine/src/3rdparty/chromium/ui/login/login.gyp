# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/login',
  },
  'targets': [
    {
      # TODO bug 512913 support in GN.
      'target_name': 'login_resources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'login_resources',
          'variables': {
            'grit_grd_file': 'login_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(SHARED_INTERMEDIATE_DIR)/ui/login/login_resources.pak',
          ],
        },
      ],
    },
  ],
}
