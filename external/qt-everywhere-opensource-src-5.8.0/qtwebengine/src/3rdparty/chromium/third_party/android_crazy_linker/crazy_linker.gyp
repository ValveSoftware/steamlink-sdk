# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN: //third_party/android_crazy_linker:android_crazy_linker
      'target_name': 'crazy_linker',
      'type': 'static_library',
      'include_dirs': [
        'src/include',
        'src/src',
      ],
      'defines': [
        'CRAZY_DEBUG=0',
      ],
      'sources': [
        'src/src/crazy_linker_api.cpp',
        'src/src/crazy_linker_ashmem.cpp',
        'src/src/crazy_linker_debug.cpp',
        'src/src/crazy_linker_elf_loader.cpp',
        'src/src/crazy_linker_elf_relocations.cpp',
        'src/src/crazy_linker_elf_relro.cpp',
        'src/src/crazy_linker_elf_symbols.cpp',
        'src/src/crazy_linker_elf_view.cpp',
        'src/src/crazy_linker_error.cpp',
        'src/src/crazy_linker_globals.cpp',
        'src/src/crazy_linker_library_list.cpp',
        'src/src/crazy_linker_library_view.cpp',
        'src/src/crazy_linker_line_reader.cpp',
        'src/src/crazy_linker_proc_maps.cpp',
        'src/src/crazy_linker_rdebug.cpp',
        'src/src/crazy_linker_search_path_list.cpp',
        'src/src/crazy_linker_shared_library.cpp',
        'src/src/crazy_linker_system.cpp',
        'src/src/crazy_linker_thread.cpp',
        'src/src/crazy_linker_util.cpp',
        'src/src/crazy_linker_wrappers.cpp',
        'src/src/crazy_linker_zip.cpp',
        'src/src/linker_phdr.cpp',
      ],
      'link_settings': {
        'libraries': [
          '-llog',
          '-ldl',
        ],
      },
      'direct_dependent_settings': {
        'include_dirs': [
          'src/include',
        ],
      },
    },
  ],
}
