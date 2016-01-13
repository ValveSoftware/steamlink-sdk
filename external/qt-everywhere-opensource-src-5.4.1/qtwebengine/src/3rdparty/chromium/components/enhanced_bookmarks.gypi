# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'enhanced_bookmarks',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../sql/sql.gyp:sql',
        '../ui/gfx/gfx.gyp:gfx',
        '../url/url.gyp:url_lib',
        'enhanced_bookmarks_proto',
      ],
      'sources': [
        'enhanced_bookmarks/image_store.cc',
        'enhanced_bookmarks/image_store.h',
        'enhanced_bookmarks/image_store_util.cc',
        'enhanced_bookmarks/image_store_util.h',
        'enhanced_bookmarks/image_store_util_ios.mm',
        'enhanced_bookmarks/metadata_accessor.cc',
        'enhanced_bookmarks/metadata_accessor.h',
        'enhanced_bookmarks/persistent_image_store.cc',
        'enhanced_bookmarks/persistent_image_store.h',
      ],
      'conditions': [
        ['OS=="ios"', {
          'sources!': [
            'enhanced_bookmarks/image_store_util.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'enhanced_bookmarks_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'enhanced_bookmarks',
      ],
      'sources': [
        'enhanced_bookmarks/test_image_store.cc',
        'enhanced_bookmarks/test_image_store.h',
      ],
    },
    {
      'target_name': 'enhanced_bookmarks_proto',
      'type': 'static_library',
      'sources': [
        'enhanced_bookmarks/proto/metadata.proto',
      ],
      'variables': {
        'proto_in_dir': './enhanced_bookmarks/proto',
        'proto_out_dir': 'components/enhanced_bookmarks/proto',
      },
      'includes': [ '../build/protoc.gypi' ],
    },
  ],
}
