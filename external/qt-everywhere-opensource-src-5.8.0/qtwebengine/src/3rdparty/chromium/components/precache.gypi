# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/precache/core
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
        'precache/core/fetcher_pool.h',
        'precache/core/precache_database.cc',
        'precache/core/precache_database.h',
        'precache/core/precache_fetcher.cc',
        'precache/core/precache_fetcher.h',
        'precache/core/precache_switches.cc',
        'precache/core/precache_switches.h',
        'precache/core/precache_session_table.cc',
        'precache/core/precache_session_table.h',
        'precache/core/precache_url_table.cc',
        'precache/core/precache_url_table.h',
      ],
      'includes': [ 'precache/precache_defines.gypi', ],
      'direct_dependent_settings': {
        # Make direct dependents also include the precache defines. This allows
        # the unit tests to use these defines.
        'includes': [ 'precache/precache_defines.gypi', ],
      },
    },
    {
      # GN version: //components/precache/core:proto
      'target_name': 'precache_core_proto',
      'type': 'static_library',
      'sources': [
        'precache/core/proto/precache.proto',
        'precache/core/proto/unfinished_work.proto',
      ],
      'variables': {
        'proto_in_dir': 'precache/core/proto',
        'proto_out_dir': 'components/precache/core/proto',
      },
      'includes': [ '../build/protoc.gypi', ],
    },
  ],
  'conditions': [
    ['OS!="ios"', {
      'targets': [
        {
          # GN Version: //components/precache/content
          'target_name': 'precache_content',
          'type': 'static_library',
          'dependencies': [
            'precache_core',
            'precache_core_proto',
            '../base/base.gyp:base',
            '../components/components.gyp:sync_driver',
            '../content/content.gyp:content_browser',
            '../url/url.gyp:url_lib',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            # Note: sources list duplicated in GN build.
            'precache/content/precache_manager.cc',
            'precache/content/precache_manager.h',
          ],
        },
      ],
    }],
    ['OS=="android"', {
      'targets': [{
        'target_name': 'precache_java',
        'type': 'none',
        'dependencies': [
          '../base/base.gyp:base',
          '../content/content.gyp:content_java',
        ],
        'variables': {
          'java_in_dir': 'precache/android/java',
        },
        'includes': [ '../build/java.gypi' ],
      }, {
        'target_name': 'precache_javatests',
        'type': 'none',
        'dependencies': [
          'precache_java',
          '../base/base.gyp:base_java_test_support',
        ],
        'variables': {
          'java_in_dir': 'precache/android/javatests',
        },
        'includes': [ '../build/java.gypi' ],
      }],
    }],
  ],
}
