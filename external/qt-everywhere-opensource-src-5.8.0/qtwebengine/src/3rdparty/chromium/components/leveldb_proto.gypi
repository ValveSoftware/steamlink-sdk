# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'leveldb_proto',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
      ],
      'sources': [
        'leveldb_proto/leveldb_database.cc',
        'leveldb_proto/leveldb_database.h',
        'leveldb_proto/proto_database.h',
        'leveldb_proto/proto_database_impl.h',
      ]
    },
    {
      'target_name': 'leveldb_proto_test_support',
      'type': 'static_library',
      'dependencies': [
        'leveldb_proto',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'leveldb_proto/testing/fake_db.h',
        'leveldb_proto/testing/proto/test.proto',
      ],
      'variables': {
        'proto_in_dir': 'leveldb_proto/testing/proto',
        'proto_out_dir': 'components/leveldb_proto/testing/proto',
      },
      'includes': [ '../build/protoc.gypi' ]
    },
  ],
}
