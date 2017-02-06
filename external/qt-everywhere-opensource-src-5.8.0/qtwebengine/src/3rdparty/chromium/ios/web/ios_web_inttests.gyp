# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ios_web_inttests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../ios/provider/ios_provider_web.gyp:ios_provider_web',
        '../../net/net.gyp:net_test_support',
        '../../services/shell/shell_public.gyp:shell_public',
        '../../testing/gtest.gyp:gtest',
        '../../ui/base/ui_base.gyp:ui_base_test_support',
        'ios_web.gyp:ios_web',
        'ios_web.gyp:ios_web_test_support',
        'ios_web.gyp:test_mojo_bindings',
        'test_resources',
        'packed_test_resources',
      ],
      'sources': [
        'browser_state_web_view_partition_inttest.mm',
        'public/test/http_server_inttest.mm',
        'test/run_all_unittests.cc',
        'webui/web_ui_mojo_inttest.mm',
      ],
      'mac_bundle_resources': [
        '<(SHARED_INTERMEDIATE_DIR)/ios/web/test/resources.pak'
      ],
    },
    {
      # GN version: //ios/web/test:resources
      'target_name': 'test_resources',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'ios_web.gyp:test_mojo_bindings',
      ],
      'actions': [
        {
          'action_name': 'test_resources',
          'variables': {
            'grit_grd_file': 'test/test_resources.grd',
            'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ios/web/test',
            'grit_additional_defines': [
              '-E', 'root_gen_dir=<(SHARED_INTERMEDIATE_DIR)',
            ],
          },
           'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(SHARED_INTERMEDIATE_DIR)/ios/web/test/test_resources.pak'
          ],
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
    },
    {
      'target_name': 'packed_test_resources',
      'type': 'none',
      'hard_depency': 1,
      'dependencies': [
        'test_resources',
      ],
      'variables': {
        'repack_path': [
          '../../tools/grit/grit/format/repack.py',
        ],
      },
      'actions': [
        {
          'action_name': 'repack_test_resources',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/ios/web/ios_web_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ios/web/test/test_resources.pak'
            ],
            'pak_output': '<(SHARED_INTERMEDIATE_DIR)/ios/web/test/resources.pak',
          },
          'includes': [ '../../build/repack_action.gypi' ],
        },
      ],
    },
  ],
}
