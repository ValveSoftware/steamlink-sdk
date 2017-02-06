# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'devtools_http_handler',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../components/components.gyp:devtools_discovery',
        '../content/content.gyp:content_browser',
        '../net/net.gyp:net',
        '../net/net.gyp:http_server',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'devtools_http_handler/devtools_http_handler.cc',
        'devtools_http_handler/devtools_http_handler.h',
        'devtools_http_handler/devtools_http_handler_delegate.h',
      ],
    },
  ],
}
