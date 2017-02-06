# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [],
  'conditions': [
    # The CrNet build is ninja-only because of the hack in
    # ios/build/packaging/link_dependencies.py.
    ['OS=="ios" and "<(GENERATOR)"=="ninja"', {
      'targets': [
        {
          'target_name': 'crnet_test',
          'type': 'executable',
          'dependencies': [
            '../../../ios/crnet/crnet.gyp:crnet',
            '../../../ios/third_party/gcdwebserver/gcdwebserver.gyp:gcdwebserver',
            '../../../testing/gtest.gyp:gtest',
          ],
          'sources': [
            'crnet_http_tests.mm',
            'crnet_test_runner.mm',
          ],
          'include_dirs': [
            '../../..',
            '..',
          ],
          'link_settings': {
          },
        },
      ],
    }],
  ],
}
