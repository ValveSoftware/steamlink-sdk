# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/mime_util/mime_util
      'target_name': 'mime_util',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../net/net.gyp:net',
      ],
      'sources': [
        'mime_util.cc',
        'mime_util.h',
      ],
      'conditions': [
        ['OS!="ios"', {
          # iOS doesn't use and must not depend on //media
          'dependencies': [
            '../../media/media.gyp:media',
          ],
        }],
      ],
    }
  ],
}
