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
      'dependencies': [
        'common/api/api.gyp:extensions_api_mojom',
        '../device/serial/serial.gyp:device_serial_mojo',
      ],
      'actions': [
        {
          'action_name': 'generate_extensions_resources',
          'variables': {
            'grit_grd_file': 'extensions_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'generate_extensions_browser_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/extensions_browser_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'generate_extensions_renderer_resources',
          'variables': {
            'grit_grd_file': 'renderer/resources/extensions_renderer_resources.grd',
            'grit_additional_defines': [
              '-E', 'mojom_root=<(SHARED_INTERMEDIATE_DIR)',
            ],
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    }
  ]
}
