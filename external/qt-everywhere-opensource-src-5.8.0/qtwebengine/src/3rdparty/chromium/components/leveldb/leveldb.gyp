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
      # GN version: //components/leveldb:lib
      'target_name': 'leveldb_lib',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'env_mojo.cc',
        'env_mojo.h',
        'leveldb_database_impl.cc',
        'leveldb_database_impl.h',
        'leveldb_mojo_proxy.cc',
        'leveldb_mojo_proxy.h',
        'leveldb_service_impl.cc',
        'leveldb_service_impl.h',
      ],
      'dependencies': [
        'leveldb_public_lib',
        '../../components/filesystem/filesystem.gyp:filesystem_lib',
        '../../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../../services/shell/shell_public.gyp:shell_public',
        '../../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
      ]
    },
    {
      # GN version: //components/leveldb/public/cpp:cpp
      'target_name': 'leveldb_public_lib',
      'type': 'static_library',
      'sources': [
        'public/cpp/remote_iterator.cc',
        'public/cpp/remote_iterator.h',
        'public/cpp/util.cc',
        'public/cpp/util.h',
      ],
      'dependencies': [
        'leveldb_bindings_mojom',
        '../../mojo/mojo_edk.gyp:mojo_system_impl',
        '../../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../../services/shell/shell_public.gyp:shell_public',
        '../../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
      ]
    },
    {
      # GN version: //components/leveldb/public/interfaces
      'target_name': 'leveldb_bindings',
      'type': 'static_library',
      'dependencies': [
        'leveldb_bindings_mojom',
      ],
    },
    {
      'target_name': 'leveldb_bindings_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'public/interfaces/leveldb.mojom',
        ],
      },
      'dependencies': [
        '../../components/filesystem/filesystem.gyp:filesystem_bindings_mojom',
      ],
      'includes': [
        '../../mojo/mojom_bindings_generator_explicit.gypi',
      ],
    }
  ],
}
