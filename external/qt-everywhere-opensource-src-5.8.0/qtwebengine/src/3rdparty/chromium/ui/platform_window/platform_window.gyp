# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [{
    # GN version: //ui/platform_window
    'target_name': 'platform_window',
    'type': 'none',
    'sources': [
      'platform_window.h',
      'platform_window_delegate.h',
    ],
  }],
}
