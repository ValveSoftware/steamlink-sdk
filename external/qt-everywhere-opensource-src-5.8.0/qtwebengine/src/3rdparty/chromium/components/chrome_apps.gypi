# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/components/chrome_apps',
  },
  'targets': [
    {
      'target_name': 'chrome_apps_resources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'chrome_apps_resources',
          'variables': {
            'grit_grd_file': 'chrome_apps/chrome_apps_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(SHARED_INTERMEDIATE_DIR)/components/chrome_apps/chrome_apps_resources.pak',
          ],
        },
      ],
    },
    {
      'target_name': 'chrome_apps',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        'chrome_apps_resources',
      ],
      'defines': [
        'CHROME_APPS_IMPLEMENTATION',
      ],
      'sources': [
        '<(grit_out_dir)/grit/chrome_apps_resources_map.cc',
        '<(grit_out_dir)/grit/chrome_apps_resources_map.h',
        'chrome_apps/chrome_apps_export.h',
        'chrome_apps/chrome_apps_resource_util.cc',
        'chrome_apps/chrome_apps_resource_util.h',
      ]
    }
  ]
}
