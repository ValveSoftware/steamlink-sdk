# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //ios/web:resources
      'target_name': 'ios_web_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ios/web',
      },
      'actions': [
        {
          'action_name': 'generate_ios_web_resources',
          'variables': {
            'grit_grd_file': 'ios_web_resources.grd',
          },
          'grit_additional_defines': [
              '-E', 'root_out_dir=<(PRODUCT_DIR)',
            ],
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(SHARED_INTERMEDIATE_DIR)/ios/web/ios_web_resources.pak'
          ],
        },
      ],
    },
  ],
}
