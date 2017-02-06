# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [],
  'conditions': [
    # Our shared library hack only works with ninja; xcode cannot generate
    # iOS build targets for dynamic libraries. More details below.
    ['"<(GENERATOR)"=="ninja"', {
      'targets': [
        {
          'target_name': 'crnet_dummy',
          'type': 'executable',
          'mac_bundle': 1,
          'dependencies': [
            '../../ios/crnet/crnet.gyp:crnet',
          ],
          'sources': [
            '../../ios/build/packaging/dummy_main.mm',
          ],
          'include_dirs': [
            '../..',
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': '../../ios/build/packaging/dummy-Info.plist',
          },
        },
        {
          # Build this target to package a standalone CrNet in a single
          # .a file.
          'target_name': 'crnet_pack',
          'type': 'none',
          'dependencies': [
            # Depend on the dummy target so that all of CrNet's dependencies
            # are built before packaging.
            'crnet_dummy',
          ],
          'actions': [
            {
              'action_name': 'Package CrNet',
              'variables': {
                'tool_path':
                    '../../ios/build/packaging/link_dependencies.py',
              },

              # Actions need an inputs list, even if it's empty.
              'inputs': [
                '<(tool_path)',
                '<(PRODUCT_DIR)/crnet_dummy.app/crnet_dummy',
              ],
              # Only specify one output, since this will be libtool's output.
              'outputs': [ '<(PRODUCT_DIR)/libcrnet_standalone.a' ],
              'action': ['<(tool_path)',
                         '<(PRODUCT_DIR)',
                         'crnet_dummy.app/crnet_dummy',
                         '<@(_outputs)',
              ],
            },
          ],
        },
      ],
    }],
  ],
}
