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
      # GN version: //services/user:lib
      'target_name': 'user_service_lib',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'user_id_map.cc',
        'user_id_map.h',
        'user_service.cc',
        'user_service.h',
        'user_shell_client.cc',
        'user_shell_client.h',
      ],
      'dependencies': [
        'user_app_manifest',
        'user_service_bindings',
        '../../base/base.gyp:base',
        '../../components/filesystem/filesystem.gyp:filesystem_lib',
        '../../components/leveldb/leveldb.gyp:leveldb_lib',
        '../../services/shell/shell_public.gyp:shell_public',
        '../../services/tracing/tracing.gyp:tracing_lib',
        '../../mojo/mojo_edk.gyp:mojo_system_impl',
        '../../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../../url/url.gyp:url_lib',
      ],
      'export_dependent_settings': [
        'user_service_bindings',
      ],
    },
    {
      # GN version: //services/user/public/interfaces
      'target_name': 'user_service_bindings',
      'type': 'static_library',
      'dependencies': [
        'user_service_bindings_mojom',
      ],
    },
    {
      'target_name': 'user_service_bindings_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'public/interfaces/user_service.mojom',
        ],
      },
      'dependencies': [
        '../../components/filesystem/filesystem.gyp:filesystem_bindings_mojom',
        '../../components/leveldb/leveldb.gyp:leveldb_bindings_mojom',
      ],
      'includes': [
        '../../mojo/mojom_bindings_generator_explicit.gypi',
      ],
    },
    {
      'target_name': 'user_service_public_lib',
      'type': 'static_library',
      'sources': [
        'public/cpp/constants.cc',
        'public/cpp/constants.h',
      ],
      'include_dirs': [
        '../..',
      ],
    },
    {
      # GN version: //services/user:manifest
      'target_name': 'user_app_manifest',
      'type': 'none',
      'variables': {
        'application_type': 'mojo',
        'application_name': 'user',
        'source_manifest': '<(DEPTH)/services/user/manifest.json',
      },
      'includes': [
        '../../mojo/public/mojo_application_manifest.gypi',
      ],
      'hard_dependency': 1,
    },
  ],
}
