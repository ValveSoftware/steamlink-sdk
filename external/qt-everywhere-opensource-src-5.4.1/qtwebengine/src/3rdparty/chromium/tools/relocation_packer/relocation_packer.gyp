# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'lib_relocation_packer',
      'toolsets': ['host'],
      'type': 'static_library',
      'dependencies': [
        '../../third_party/elfutils/elfutils.gyp:libelf',
      ],
      'sources': [
        'src/debug.cc',
        'src/elf_file.cc',
        'src/leb128.cc',
        'src/packer.cc',
        'src/run_length_encoder.cc',
      ],
    },
    {
      'target_name': 'relocation_packer',
      'toolsets': ['host'],
      'type': 'executable',
      'dependencies': [
        '../../third_party/elfutils/elfutils.gyp:libelf',
        'lib_relocation_packer',
      ],
      'sources': [
        'src/main.cc',
      ],
    },
    {
      'target_name': 'relocation_packer_unittests',
      'toolsets': ['host'],
      'type': 'executable',
      'cflags': [
        '-DINTERMEDIATE_DIR="<(INTERMEDIATE_DIR)"',
      ],
      'dependencies': [
        '../../testing/gtest.gyp:gtest',
        'lib_relocation_packer',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'src/elf_file_unittest.cc',
        'src/leb128_unittest.cc',
        'src/packer_unittest.cc',
        'src/run_length_encoder_unittest.cc',
        'src/run_all_unittests.cc',
      ],
      'copies': [
        {
          'destination': '<(INTERMEDIATE_DIR)',
          'files': [
            'test_data/elf_file_unittest_relocs.so',
            'test_data/elf_file_unittest_relocs_packed.so',
          ],
        },
      ],
    },
  ],
}
