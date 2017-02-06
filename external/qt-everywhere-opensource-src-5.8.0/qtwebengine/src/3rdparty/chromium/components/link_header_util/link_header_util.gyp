# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/link_header_util/link_header_util
      'target_name': 'link_header_util',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../net/net.gyp:net',
      ],
      'sources': [
        'link_header_util.cc',
        'link_header_util.h',
      ],
    }
  ],
}
