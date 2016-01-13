# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'ashmem',
      'conditions': [
        ['android_webview_build==1', {
          # WebView must use the Android system version of ashmem to avoid
          # linking problems.
          'type': 'none',
          'variables': {
            'headers_root_path': '.',
            'header_filenames': [ 'ashmem.h' ],
            'shim_generator_additional_args': [
              '--prefix', 'cutils/',
            ],
          },
          'includes': [
            '../../build/shim_headers.gypi',
          ],
          'link_settings': {
            'libraries': [
              '-lcutils',
            ],
          },
        }, {
          'type': 'static_library',
          'sources': [
            'ashmem.h',
            'ashmem-dev.c'
          ],
        }],
      ],
    },
  ],
}
