# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/win_precompile.gypi',
  ],
  'targets': [
    {
      'target_name': 'ced',
      'type': 'static_library',
      'include_dirs': [
        'src',
      ],
      'sources': [
        "src/compact_enc_det/compact_enc_det.cc",
        "src/compact_enc_det/compact_enc_det.h",
        "src/compact_enc_det/compact_enc_det_generated_tables.h",
        "src/compact_enc_det/compact_enc_det_generated_tables2.h",
        "src/compact_enc_det/compact_enc_det_hint_code.cc",
        "src/compact_enc_det/compact_enc_det_hint_code.h",
        "src/compact_enc_det/detail_head_string.inc",
        "src/util/basictypes.h",
        "src/util/build_config.h",
        "src/util/commandlineflags.h",
        "src/util/encodings/encodings.cc",
        "src/util/encodings/encodings.h",
        "src/util/encodings/encodings.pb.h",
        "src/util/languages/languages.cc",
        "src/util/languages/languages.h",
        "src/util/languages/languages.pb.h",
        "src/util/logging.h",
        "src/util/port.h",
        "src/util/string_util.h",
        "src/util/varsetter.h",
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'direct_dependent_settings': {
            'defines': [
              'COMPILER_MSVC',
            ],
          },
          'msvs_disabled_warnings': [4005, 4006, 4018, 4244, 4309, 4800, 4267],
        }, {
          'direct_dependent_settings': {
            'defines': [
              'COMPILER_GCC',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'ced_unittests',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/testing/gtest.gyp:gtest_main',
        'ced',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'sources': [
        "src/compact_enc_det/compact_enc_det_fuzz_test.cc",
        "src/compact_enc_det/compact_enc_det_unittest.cc",
      ],
    },
  ],
}
