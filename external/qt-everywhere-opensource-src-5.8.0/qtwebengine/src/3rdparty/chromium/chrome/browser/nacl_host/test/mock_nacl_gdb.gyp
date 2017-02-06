# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    ['disable_nacl==0 and (OS=="win" or OS=="linux")', {
      'targets': [
        {
          'target_name': 'mock_nacl_gdb',
          'type': 'executable',
          'include_dirs': [
            '../../../../',
          ],
          'sources': [
            'mock_nacl_gdb.cc',
          ],
          'dependencies': [
             '../../../../base/base.gyp:base',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        },
      ],
    }],
  ],
}
