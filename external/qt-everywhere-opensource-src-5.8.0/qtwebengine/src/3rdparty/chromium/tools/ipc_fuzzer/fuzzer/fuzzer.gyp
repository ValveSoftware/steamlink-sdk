# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ipc_fuzzer',
      'type': 'executable',
      'dependencies': [
        '../message_lib/message_lib.gyp:ipc_message_lib',
      ],
      'sources': [
        'fuzzer.h',
        'fuzzer.cc',
        'fuzzer_main.cc',
        'generator.h',
        'generator.cc',
        'mutator.h',
        'mutator.cc',
        'rand_util.h',
        'rand_util.cc',
      ],
      'conditions': [
        ['asan==1 and chromeos==1', {
          'cflags!': [
            # Compiling fuzzer.cc with ASan takes too long, see
            # http://crbug.com/360158.
            '-fsanitize=address',
            '-fsanitize-coverage=<(sanitizer_coverage)',
          ],
        }],
      ],
      'include_dirs': [
        '../../..',
      ],
      'defines': [
        'USE_CUPS',
      ],
    },
  ],
}
