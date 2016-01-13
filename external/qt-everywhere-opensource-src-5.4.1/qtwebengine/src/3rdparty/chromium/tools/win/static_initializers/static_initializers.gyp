# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'conditions': [
    ['not msvs_express', {
      'targets': [
        {
          'target_name': 'static_initializers',
          'type': 'executable',
          'sources': [
            'static_initializers.cc',
          ],
          'include_dirs': [
            '$(VSInstallDir)/DIA SDK/include',
          ],
        },
      ],
    }, {
      'targets': [],
    }],
  ]
}

