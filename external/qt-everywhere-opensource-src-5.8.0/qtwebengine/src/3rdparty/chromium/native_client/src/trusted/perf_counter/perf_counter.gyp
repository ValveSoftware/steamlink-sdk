# Copyright 2011 (c) The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # ----------------------------------------------------------------------
  # Default settings
  # ----------------------------------------------------------------------

  'includes': [
    '../../../build/common.gypi',
    ],


  # ----------------------------------------------------------------------
  # actual targets
  # ----------------------------------------------------------------------
  'targets': [
    {
      'target_name': 'nacl_perf_counter',
      'type': 'static_library',
      'sources': [
        'nacl_perf_counter.c',
      ],
    },
  ],

  # Version for windows 64.
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'nacl_perf_counter64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            'nacl_perf_counter.c',
          ],
        },
      ],
    }],
  ],
}
