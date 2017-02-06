# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule to invoke
# the flatc compiler in a consistent manner.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my_flatc_lib',
#   'type': 'static_library',
#   'sources': [
#     'foo.fbs',
#   ],
#   'variables': {
#     'flatc_out_dir': 'dir/for/my_flatc_lib'
#   },
#   'includes': ['path/to/this/gypi/file'],
#   'dependencies': [
#     '<(DEPTH)/third_party/flatbuffers/flatbuffers.gyp:flatbuffers',
#   ]
# }
{
  'variables': {
    'flatc': '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)flatc<(EXECUTABLE_SUFFIX)',
    'cc_dir': '<(SHARED_INTERMEDIATE_DIR)/flatc_out/<(flatc_out_dir)',
  },
  'rules': [
    {
      'rule_name': 'genflatc',
      'extension': 'fbs',
      'inputs': [
        '<(flatc)',
      ],
      'outputs': [
        '<(cc_dir)/<(RULE_INPUT_ROOT)_generated.h',
      ],
      'action': [
        '<(flatc)',
	'-c',
	'-o',
	'<(cc_dir)',
	'<(RULE_INPUT_PATH)'
      ],
      'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
      'process_outputs_as_sources': 1,
    }
  ],
  'dependencies': [
    '<(DEPTH)/third_party/flatbuffers/flatbuffers.gyp:flatc',
  ],
  'include_dirs': [
     '<(SHARED_INTERMEDIATE_DIR)/flatc_out',
     '<(DEPTH)',
  ],
  'direct_dependent_settings': {
    'include_dirs': [
      '<(SHARED_INTERMEDIATE_DIR)/flatc_out',
      '<(DEPTH)',
    ]
  },
  'export_dependent_settings': [
    # The generated headers reference headers within flatbuffers,
    # so dependencies must be able to find those headers too.
    '<(DEPTH)/third_party/flatbuffers/flatbuffers.gyp:flatbuffers',
  ],
  # This target exports a hard dependency because it generates header
  # files.
  'hard_dependency': 1,
}
