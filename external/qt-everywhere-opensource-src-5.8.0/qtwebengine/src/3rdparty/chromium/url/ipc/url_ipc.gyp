# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
     {
      # GN version: //url/ipc
      'target_name': 'url_ipc',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../ipc/ipc.gyp:ipc',
        '../../url/url.gyp:url_lib',
      ],
      'defines': [ 'URL_IPC_IMPLEMENTATION' ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'url_ipc_export.h',
        'url_param_traits.cc',
        'url_param_traits.h',
      ],
    },
    {
      'target_name': 'url_ipc_unittests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:run_all_unittests',
        '../../ipc/ipc.gyp:test_support_ipc',
        '../../testing/gtest.gyp:gtest',
        '../url.gyp:url_lib',
        'url_ipc',
      ],
      'sources': [ 'url_param_traits_unittest.cc' ],
    },
  ],
}
