# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/about_handler
      'target_name': 'about_handler',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'about_handler/about_protocol_handler.cc',
        'about_handler/about_protocol_handler.h',
        'about_handler/url_request_about_job.cc',
        'about_handler/url_request_about_job.h',
      ],
    },
  ],
}
