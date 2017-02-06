# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'blacklist',
      'type': 'static_library',
      'include_dirs': [
        '..',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'sources': [
        'blacklist/blacklist.cc',
        'blacklist/blacklist.h',
        'blacklist/blacklist_interceptions.cc',
        'blacklist/blacklist_interceptions.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../chrome_elf/chrome_elf.gyp:chrome_elf_breakpad',
        '../chrome_elf/chrome_elf.gyp:chrome_elf_constants',
        '../sandbox/sandbox.gyp:sandbox',
      ],
    },
    {
      'target_name': 'blacklist_test_main_dll',
      'type': 'shared_library',
      'sources': [
        'blacklist/test/blacklist_test_main_dll.cc',
        'blacklist/test/blacklist_test_main_dll.def',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'blacklist',
      ],
    },
    {
      'target_name': 'blacklist_test_dll_1',
      'type': 'loadable_module',
      'sources': [
        'blacklist/test/blacklist_test_dll_1.cc',
        'blacklist/test/blacklist_test_dll_1.def',
      ],
    },
    {
      'target_name': 'blacklist_test_dll_2',
      'type': 'loadable_module',
      'sources': [
        'blacklist/test/blacklist_test_dll_2.cc',
        'blacklist/test/blacklist_test_dll_2.def',
      ],
    },
    {
      'target_name': 'blacklist_test_dll_3',
      'type': 'loadable_module',
      'sources': [
        'blacklist/test/blacklist_test_dll_3.cc',
      ],
      'msvs_settings': {
        # There's no exports in this DLL, this tells ninja not to expect an
        # import lib so that it doesn't keep rebuilding unnecessarily due to
        # the .lib being "missing".
        'NoImportLibrary': 'true',
      },
    },
  ],
}

