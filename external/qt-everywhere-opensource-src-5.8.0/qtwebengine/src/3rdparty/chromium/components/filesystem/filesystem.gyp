# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # This turns on e.g. the filename-based detection of which
    # platforms to include source files on (e.g. files ending in
    # _mac.h or _mac.cc are only compiled on MacOSX).
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //components/filesystem:lib
      'target_name': 'filesystem_lib',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'directory_impl.cc',
        'directory_impl.h',
        'file_impl.cc',
        'file_impl.h',
        'file_system_impl.cc',
        'file_system_impl.h',
        'lock_table.cc',
        'lock_table.h',
        'shared_temp_dir.cc',
        'shared_temp_dir.h',
        'util.cc',
        'util.h',
      ],
      'dependencies': [
        'filesystem_bindings',
        '../../mojo/mojo_edk.gyp:mojo_system_impl',
        '../../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../../mojo/mojo_public.gyp:mojo_cpp_system',
        '../../services/shell/shell_public.gyp:shell_public',
        '../../url/url.gyp:url_lib',
      ],
      'export_dependent_settings': [
        'filesystem_bindings',
      ],
    },
    {
      # GN version: //components/filesystem/public/interfaces
      'target_name': 'filesystem_bindings',
      'type': 'static_library',
      'dependencies': [
        'filesystem_bindings_mojom',
      ],
      'hard_dependency': 1,
    },
    {
      'target_name': 'filesystem_bindings_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'public/interfaces/directory.mojom',
          'public/interfaces/file.mojom',
          'public/interfaces/file_system.mojom',
          'public/interfaces/types.mojom',
        ],
      },
      'includes': [
        '../../mojo/mojom_bindings_generator_explicit.gypi',
      ],
    }
  ],
}
