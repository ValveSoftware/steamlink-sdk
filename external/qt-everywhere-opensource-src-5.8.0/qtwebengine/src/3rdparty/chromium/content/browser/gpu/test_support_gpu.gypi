# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into targets which run gpu tests.
{
  'variables': {
     'test_list_out_dir': '<(SHARED_INTERMEDIATE_DIR)/content/test/gpu',
     'src_dir': '../../..',
  },
  'defines': [
    'HAS_OUT_OF_PROC_TEST_RUNNER',
  ],
  'include_dirs': [
    '<(src_dir)',
    '<(test_list_out_dir)',
  ],
  # hard_dependency is necessary for this target because it has actions
  # that generate a header file included by dependent targets. The header
  # file must be generated before the dependents are compiled. The usual
  # semantics are to allow the two targets to build concurrently.
  'hard_dependency': 1,
  'conditions': [
    ['OS=="win"', {
      'include_dirs': [
        '<(DEPTH)/third_party/wtl/include',
      ],
      'configurations': {
        'Debug': {
          'msvs_settings': {
            'VCLinkerTool': {
              'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
            },
          },
        },
      },
    }],
    ['OS=="mac"', {
      # See comments about "xcode_settings" elsewhere in this file.
      'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
    }],
    ['chromeos==1 or (OS=="linux" and use_aura==1)', {
      'dependencies': [
        '<(src_dir)/build/linux/system.gyp:nss',
      ],
    }],
    ['toolkit_views==1', {
      'dependencies': [
       '<(src_dir)/ui/views/views.gyp:views',
      ],
    }],
  ],
}
