# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Rule to extract integer values for each symbol from an object file.
# The output file name is the input file name with extension replaced with
# asm or h.
# The following gyp variables must be set before including this gypi:
#   output_format, the output format of integer value.
#   output_dir, the full path where the output file should be created.
#
# For example:
#
# 'sources': ['a.o', 'b.o'],
# 'variables': {
#   'output_format': 'cheader',
#   'output_dir': 'output',
# },
# 'includes': ['obj_int_extract.gypi'],
#
# This extracts the symbol from a.o and b.o, and outputs them to a.h and b.h
# in output directory.
{
  'variables': {
    'conditions': [
      ['os_posix==1', {
        'asm_obj_extension': 'o',
      }],
      ['OS=="win"', {
        'asm_obj_extension': 'obj',
      }],
      ['output_format=="cheader"', {
        'output_extension': 'h',
      }, {
        'output_extension': 'asm',
      }],
    ],
  },
  'rules': [
    {
      'rule_name': 'obj_int_extract',
      'extension': '<(asm_obj_extension)',
      'inputs': [
        '<(PRODUCT_DIR)/libvpx_obj_int_extract',
        'obj_int_extract.py',
      ],
      'outputs': [
        '<(output_dir)/<(RULE_INPUT_ROOT).<(output_extension)',
      ],
      'action': [
	'python',
        '<(DEPTH)/third_party/libvpx/obj_int_extract.py',
        '-e', '<(PRODUCT_DIR)/libvpx_obj_int_extract',
        '-f', '<(output_format)',
        '-b', '<(RULE_INPUT_PATH)',
        '-o', '<(output_dir)/<(RULE_INPUT_ROOT).<(output_extension)',
      ],
      'message': 'Generate assembly offsets <(RULE_INPUT_PATH)',
    },
  ],
}
