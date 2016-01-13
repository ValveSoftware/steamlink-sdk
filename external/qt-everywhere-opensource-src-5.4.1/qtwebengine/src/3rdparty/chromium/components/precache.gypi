# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'precache_content',
      'type': 'static_library',
      'dependencies': [
        'precache_core',
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'precache/content/precache_manager.cc',
        'precache/content/precache_manager.h',
        'precache/content/precache_manager_factory.cc',
        'precache/content/precache_manager_factory.h',
      ],
    },
    {
      'target_name': 'precache_core',
      'type': 'static_library',
      'dependencies': [
        'precache_core_proto',
        '../base/base.gyp:base',
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'precache/core/precache_database.cc',
        'precache/core/precache_database.h',
        'precache/core/precache_fetcher.cc',
        'precache/core/precache_fetcher.h',
        'precache/core/precache_switches.cc',
        'precache/core/precache_switches.h',
        'precache/core/precache_url_table.cc',
        'precache/core/precache_url_table.h',
        'precache/core/url_list_provider.h',
      ],
      'includes': [ 'precache/precache_defines.gypi', ],
      'direct_dependent_settings': {
        # Make direct dependents also include the precache defines. This allows
        # the unit tests to use these defines.
        'includes': [ 'precache/precache_defines.gypi', ],
      },
    },
    {
      'target_name': 'precache_core_proto',
      'type': 'static_library',
      'sources': [
        'precache/core/proto/precache.proto',
      ],
      'variables': {
        'proto_in_dir': 'precache/core/proto',
        'proto_out_dir': 'components/precache/core/proto',
      },
      'includes': [ '../build/protoc.gypi', ],
    },
  ],
}
