# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'tld_cleanup_util',
      'type': 'static_library',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../url/url.gyp:url_lib',
      ],
      'sources': [
        'tld_cleanup_util.h',
        'tld_cleanup_util.cc',
      ],
    },
  ],
}
