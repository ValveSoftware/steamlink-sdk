# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a GYP include file that contains directives to build the
# crazy_linker sources as _part_ of another target.

{
  'targets': [
    {
      'target_name': 'crazy_linker',
      'type': 'static_library',
      'include_dirs': [
        'sources/android/crazy_linker/include',
        'sources/android/crazy_linker/src',
      ],
      'defines': [
        'CRAZY_DEBUG=0',
      ],
      'sources': [
        'sources/android/crazy_linker/src/crazy_linker_api.cpp',
        'sources/android/crazy_linker/src/crazy_linker_ashmem.cpp',
        'sources/android/crazy_linker/src/crazy_linker_debug.cpp',
        'sources/android/crazy_linker/src/crazy_linker_elf_loader.cpp',
        'sources/android/crazy_linker/src/crazy_linker_elf_relocations.cpp',
        'sources/android/crazy_linker/src/crazy_linker_elf_relro.cpp',
        'sources/android/crazy_linker/src/crazy_linker_elf_symbols.cpp',
        'sources/android/crazy_linker/src/crazy_linker_elf_view.cpp',
        'sources/android/crazy_linker/src/crazy_linker_error.cpp',
        'sources/android/crazy_linker/src/crazy_linker_globals.cpp',
        'sources/android/crazy_linker/src/crazy_linker_library_list.cpp',
        'sources/android/crazy_linker/src/crazy_linker_library_view.cpp',
        'sources/android/crazy_linker/src/crazy_linker_line_reader.cpp',
        'sources/android/crazy_linker/src/crazy_linker_proc_maps.cpp',
        'sources/android/crazy_linker/src/crazy_linker_rdebug.cpp',
        'sources/android/crazy_linker/src/crazy_linker_search_path_list.cpp',
        'sources/android/crazy_linker/src/crazy_linker_shared_library.cpp',
        'sources/android/crazy_linker/src/crazy_linker_system.cpp',
        'sources/android/crazy_linker/src/crazy_linker_thread.cpp',
        'sources/android/crazy_linker/src/crazy_linker_util.cpp',
        'sources/android/crazy_linker/src/crazy_linker_wrappers.cpp',
        'sources/android/crazy_linker/src/linker_phdr.cpp',
      ],
      'link_settings': {
        'libraries': [
          '-llog',
          '-ldl',
        ],
      },
      'direct_dependent_settings': {
        'include_dirs': [
          'sources/android/crazy_linker/include',
        ],
      },
    }, 
  ],
}
