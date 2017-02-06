# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # This turns on e.g. the filename-based detection of which
    # platforms to include source files on (e.g. files ending in
    # _mac.h or _mac.cc are only compiled on MacOSX).
    'chromium_code': 1,
  },
  'includes': [
    'javascript_dialog_extensions_client.gypi',
  ],
  'conditions': [
    ['toolkit_views==1', {
      'includes': [
        'native_app_window.gypi',
      ],
    }],
  ],
}
