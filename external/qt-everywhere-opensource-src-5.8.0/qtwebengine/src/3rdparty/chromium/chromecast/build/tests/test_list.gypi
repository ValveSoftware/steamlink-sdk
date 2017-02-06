# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'variables': {
      'variables': {
        # Note: priority can be set from 1 to 9 to indicate precedence for
        # filters (larger overrides smaller).
        'priority%': 1,
        'test_output_folder': '<(SHARED_INTERMEDIATE_DIR)/chromecast/tests',
      },
      # Copy from previous level.
      'test_output_folder': '<(test_output_folder)',

      'test_output_file_prefix':
          '<(test_output_folder)/<(priority)-<(_target_name)',
    },
    # Copy from previous level.
    'test_output_folder': '<(test_output_folder)',

    'test_filters_output_file': '<(test_output_file_prefix).filters',
    'test_generator_py': '<(DEPTH)/chromecast/tools/build/generate_test_lists.py',
    'test_list_output_file': '<(test_output_file_prefix).tests',
    'filters%': [],
  },
  'actions': [
    {
      'action_name': 'generate_test_list',
      'inputs': ['<(test_generator_py)'],
      'outputs': ['<(test_list_output_file)'],
      'message': 'Generating test list from <(_target_name)',
      'action': [
        'python', '<@(_inputs)',
        '-o', '<(test_list_output_file)',
        'create_list',
        '<@(_dependencies)'
      ],
    },
    {
      'action_name': 'generate_filter_list',
      'inputs': ['<(test_generator_py)'],
      'outputs': ['<(test_filters_output_file)'],
      'message': 'Generating filter list from <(_target_name)',
      'action': [
        'python', '<@(_inputs)',
        '-o', '<(test_filters_output_file)',
        'create_list',
        '<@(filters)'
      ],
    },
  ],
}
