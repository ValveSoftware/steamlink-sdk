# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# for translating .asm files to .S files using ads2gas.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my_lib',
#   'sources': [
#     'foo.asm',
#     'bar.c',
#   ],
#   'includes': [ 'ads2gas.gypi' ],
# }
{
  'variables': {
    # Location of the intermediate output.
    'shared_generated_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/libvpx',
    'variables': {
      'libvpx_source%': 'source/libvpx',
      'conditions': [
        ['OS=="ios"', {
          'ads2gas_script%': 'ads2gas_apple.pl',
        }, {
          'ads2gas_script%': 'ads2gas.pl',
        }],
      ],
    },
    'ads2gas_script%': '<(ads2gas_script)',
    'ads2gas_script_dir': '<(libvpx_source)/build/make',
  },
  'rules': [
    {
      'rule_name': 'convert_asm',
      'extension': 'asm',
      'inputs': [
        '<(ads2gas_script_dir)/<(ads2gas_script)',
        '<(ads2gas_script_dir)/thumb.pm',
      ],
      'outputs': [
        '<(shared_generated_dir)/<(RULE_INPUT_ROOT).S',
      ],
      'action': [
        'bash',
        '-c',
        'cat <(RULE_INPUT_PATH) | perl <(ads2gas_script_dir)/<(ads2gas_script) -chromium > <(shared_generated_dir)/<(RULE_INPUT_ROOT).S',
      ],
      'process_outputs_as_sources': 1,
      'message': 'Convert libvpx asm file for ARM <(RULE_INPUT_PATH)',
    },
  ],
}
