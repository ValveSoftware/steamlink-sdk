# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/file_manager',
  },
  'targets': [
    {
      'target_name': 'file_manager_resources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'file_manager_resources',
          'variables': {
            'grit_grd_file': 'file_manager_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(SHARED_INTERMEDIATE_DIR)/ui/file_manager/file_manager_resources.pak',
          ],
        },
      ],
    },
    {
      'target_name': 'file_manager',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        'file_manager_resources',
      ],
      'defines': [
        'FILE_MANAGER_IMPLEMENTATION',
      ],
      'sources': [
        'file_manager_export.h',
        'file_manager_resource_util.cc',
        'file_manager_resource_util.h',
        '<(grit_out_dir)/grit/file_manager_resources_map.cc',
        '<(grit_out_dir)/grit/file_manager_resources_map.h',
      ]
    },
  ],
}
