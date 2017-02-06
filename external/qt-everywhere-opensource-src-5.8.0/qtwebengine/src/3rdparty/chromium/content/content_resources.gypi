# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //content:resources
      'target_name': 'content_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/content',
      },
      'actions': [
        {
          'action_name': 'generate_content_resources',
          'variables': {
            'grit_grd_file': 'content_resources.grd',
            'grit_additional_defines': [
              '-E', 'root_out_dir=<(PRODUCT_DIR)',
            ],
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak'
          ],
        },
      ],
      'dependencies': [
        '<(DEPTH)/services/shell/shell.gyp:catalog_manifest',
        'content_app_browser_manifest',
        'content_app_gpu_manifest',
        'content_app_renderer_manifest',
        'content_app_utility_manifest',
      ],
    },
  ],
}
