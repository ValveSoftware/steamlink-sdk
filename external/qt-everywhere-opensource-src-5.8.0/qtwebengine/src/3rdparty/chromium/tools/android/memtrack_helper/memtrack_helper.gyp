# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'memtrack_helper-unstripped',
      'type': 'executable',
      # Unwind tables create a dependency on libc++. By removing them
      # the final binary will not require anything more than libc and libdl.
      # This makes its deployment easier in component=shared_library mode.
      'cflags!': [
        '-funwind-tables',
        '-fasynchronous-unwind-tables',
      ],
      'cflags': [
        '-fno-unwind-tables',
        '-fno-asynchronous-unwind-tables',
      ],
      'sources': [
        'memtrack_helper.c',
      ],
      'include_dirs': [
        '../../..',
      ],
    },
    {
      'target_name': 'memtrack_helper',
      'type': 'none',
      'dependencies': [
        'memtrack_helper-unstripped',
      ],
      'actions': [
        {
          'action_name': 'strip_memtrack_helper',
          'inputs': ['<(PRODUCT_DIR)/memtrack_helper-unstripped'],
          'outputs': ['<(PRODUCT_DIR)/memtrack_helper'],
          'action': [
            '<(android_strip)',
            '--strip-unneeded',
            '<@(_inputs)',
            '-o',
            '<@(_outputs)',
          ],
        },
      ],
    },
    {
      # Testing only, not typically pushed, hence not worth stripping.
      'target_name': 'memtrack_helper_test_client',
      'type': 'executable',
      'sources': [
        'memtrack_helper_test_client.c',
      ],
      'include_dirs': [
        '../../..',
      ],
    },
  ],
}
