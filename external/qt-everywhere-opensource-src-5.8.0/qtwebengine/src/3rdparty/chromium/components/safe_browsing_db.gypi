# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/safe_browsing_db:safe_browsing_db_shared
      'target_name': 'safe_browsing_db_shared',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../components/components.gyp:metrics',
        '../crypto/crypto.gyp:crypto',
        ':safebrowsing_proto',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'safe_browsing_db/database_manager.h',
        'safe_browsing_db/database_manager.cc',
        'safe_browsing_db/hit_report.h',
        'safe_browsing_db/hit_report.cc',
        'safe_browsing_db/prefix_set.h',
        'safe_browsing_db/prefix_set.cc',
        'safe_browsing_db/util.h',
        'safe_browsing_db/util.cc',
        'safe_browsing_db/v4_protocol_manager_util.h',
        'safe_browsing_db/v4_protocol_manager_util.cc',
        'safe_browsing_db/v4_get_hash_protocol_manager.h',
        'safe_browsing_db/v4_get_hash_protocol_manager.cc',
      ],
      'include_dirs': [
        '..',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      # GN version: //components/safe_browsing_db
      'target_name': 'safe_browsing_db',
      'type': 'static_library',
      'dependencies': [
        ':safe_browsing_db_shared',
        ':v4_store_proto',
      ],
      'sources': [
        'safe_browsing_db/v4_database.h',
        'safe_browsing_db/v4_database.cc',
        'safe_browsing_db/v4_local_database_manager.h',
        'safe_browsing_db/v4_local_database_manager.cc',
        'safe_browsing_db/v4_store.h',
        'safe_browsing_db/v4_store.cc',
        'safe_browsing_db/v4_update_protocol_manager.h',
        'safe_browsing_db/v4_update_protocol_manager.cc',
      ],
      'include_dirs': [
        '..',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      # GN version: //components/safe_browsing_db:safe_browsing_db_mobile
      'target_name': 'safe_browsing_db_mobile',
      'type': 'static_library',
      'dependencies': [
        ':safe_browsing_db_shared',
        ':safe_browsing_metadata_proto',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'safe_browsing_db/remote_database_manager.h',
        'safe_browsing_db/remote_database_manager.cc',
        'safe_browsing_db/safe_browsing_api_handler.h',
        'safe_browsing_db/safe_browsing_api_handler.cc',
        'safe_browsing_db/safe_browsing_api_handler_util.h',
        'safe_browsing_db/safe_browsing_api_handler_util.cc',
      ],
      'include_dirs': [
        '..',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      # GN version: //components/safe_browsing_db:safebrowsing_proto
      # Protobuf compiler / generator for the Safe Browsing protocol buffer.
      'target_name': 'safebrowsing_proto',
      'type': 'static_library',
      'sources': [ 'safe_browsing_db/safebrowsing.proto' ],
      'variables': {
        'proto_in_dir': 'safe_browsing_db',
        'proto_out_dir': 'components/safe_browsing_db',
      },
      'includes': [ '../build/protoc.gypi' ]
    },
    {
      # Protobuf compiler / generator for the safebrowsing full hash metadata
      # protocol buffer.
      # GN version: //components/safe_browsing_db:metadata_proto
      'target_name': 'safe_browsing_metadata_proto',
      'type': 'static_library',
      'sources': [ 'safe_browsing_db/metadata.proto' ],
      'variables': {
        'proto_in_dir': 'safe_browsing_db',
        'proto_out_dir': 'components/safe_browsing_db',
      },
      'includes': [ '../build/protoc.gypi' ]
    },
    {
      # GN version: //components/safe_browsing_db:v4_store_proto
      # Protobuf compiler / generator for the Safe Browsing protocol buffer for
      # storing hash-prefixes on disk.
      'target_name': 'v4_store_proto',
      'type': 'static_library',
      'dependencies': [
        ':safebrowsing_proto',
      ],
      'sources': [ 'safe_browsing_db/v4_store.proto' ],
      'variables': {
        'proto_in_dir': 'safe_browsing_db',
        'proto_out_dir': 'components/safe_browsing_db',
      },
      'includes': [ '../build/protoc.gypi' ]
    },
    {
      # GN version: //components/safe_browsing_db:test_database_manager
      'target_name': 'test_database_manager',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        ':safe_browsing_db',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'safe_browsing_db/test_database_manager.h',
        'safe_browsing_db/test_database_manager.cc',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
}
