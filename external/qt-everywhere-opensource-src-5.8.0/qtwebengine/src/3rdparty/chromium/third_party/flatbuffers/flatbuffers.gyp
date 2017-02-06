# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'compiler_files',
      'type': 'static_library',
      'include_dirs': [
        'src/include',
      ],
      'sources': [
        'src/src/idl_gen_cpp.cpp',
        'src/src/idl_gen_fbs.cpp',
        'src/src/idl_gen_general.cpp',
        'src/src/idl_gen_go.cpp',
        'src/src/idl_gen_js.cpp',
        'src/src/idl_gen_php.cpp',
        'src/src/idl_gen_python.cpp',
        'src/src/idl_gen_text.cpp',
        'src/src/idl_parser.cpp',
        'src/src/reflection.cpp',
        'src/src/util.cpp',
        'src/include/flatbuffers/code_generators.h',
        'src/include/flatbuffers/idl.h',
        'src/include/flatbuffers/util.h',
      ],
    },
    {
      'target_name': 'flatbuffers',
      'type': 'static_library',
      'include_dirs': [
        'src/include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src/include',
        ],
      },
      'sources': [
        'src/include/flatbuffers/flatbuffers.h',
        'src/include/flatbuffers/hash.h',
      ]
    },
    {
      'target_name': 'flatc',
      'type': 'executable',
      'dependencies': [
        'compiler_files',
        'flatbuffers',
      ],
      'sources': [
        'src/src/flatc.cpp',
      ],
    },

    # The following is just for testing.
    {
      'target_name': 'flatbuffers_samplebuffer',
      'type': 'static_library',
      'sources': [
        # Disabled as workaround for crbug.com/611351.
        # 'src/tests/include_test1.fbs',
        # 'src/tests/include_test2.fbs',
        'src/tests/monster_test.fbs',
        'src/tests/namespace_test/namespace_test1.fbs',
        'src/tests/namespace_test/namespace_test2.fbs',
      ],
      'variables': {
        'flatc_out_dir': '.',
      },
      'includes': [ 'flatc.gypi' ],
      'dependencies': [
        ':flatbuffers',
      ],
    },
    {
      # Note that you need to execute this test from the flatbuffers directory:
      # cd third_party/flatbuffers/ && ../../out/Debug/flatbuffers_unittest
      'target_name': 'flatbuffers_unittest',
      'type': 'executable',
      'dependencies': [
	'compiler_files',
        'flatbuffers'
      ],
      'sources': [
        # The following files are not included in :flatbuffers
        # but are listed here because test.cpp tests more than
        # what will get included into Chrome (reflection and generation).
        'src/include/reflection_generated.h',
        'src/include/reflection.h',
        # This is the actual test.
        'src/tests/test.cpp',
      ]
    }
  ],
}
