# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../ppapi/ppapi_nacl_test_common.gypi',
  ],
  'targets': [
    {
      'target_name': 'ppapi_tests_extensions_background_keepalive',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_tests_extensions_background_keepalive',
        # Only newlib build is used in tests, no need to build others.
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'test_data/ppapi/tests/extensions/background_keepalive',
        'sources': [
          'background_keepalive/background.cc',
        ],
        'test_files': [
          'background_keepalive/background.js',
          'background_keepalive/manifest.json',
        ],
      },
    },
    {
      'target_name': 'ppapi_tests_extensions_media_galleries',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_tests_extensions_media_galleries',
        # Only newlib build is used in tests, no need to build others.
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'test_data/ppapi/tests/extensions/media_galleries',
        'sources': [
          'media_galleries/test_galleries.cc',
          '<(DEPTH)/ppapi/tests/test_utils.cc',
          '<(DEPTH)/ppapi/tests/test_utils.h',
        ],
        'test_files': [
          'media_galleries/background.js',
          'media_galleries/index.html',
          'media_galleries/manifest.json',
          'media_galleries/test.js',
        ],
      },
    },
  ],
}
