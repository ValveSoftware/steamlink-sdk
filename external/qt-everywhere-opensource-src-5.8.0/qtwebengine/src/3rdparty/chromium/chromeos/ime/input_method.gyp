# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'input_method_out_dir':
      '<(SHARED_INTERMEDIATE_DIR)/chromeos/ime',
  },
  'targets': [
    {
      'target_name': 'gencode',
      'type': 'none',
      'actions': [
        {
          'inputs': [
            'input_methods.txt',
            'gen_input_methods.py',
          ],
          'outputs': [
            '<(input_method_out_dir)/input_methods.h',
          ],
          'action_name': 'gen_input_methods',
          'action': [
            'python',
            'gen_input_methods.py',
            'input_methods.txt',
            '<(input_method_out_dir)/input_methods.h',
          ],
          'message': 'Generating input_methods.h',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
    },
  ]
}
