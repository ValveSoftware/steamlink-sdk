# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/rlz
      'target_name': 'rlz',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../rlz/rlz.gyp:rlz_lib',
        'google_core_browser',
      ],
      'sources': [
        'rlz/rlz_tracker.cc',
        'rlz/rlz_tracker.h',
        'rlz/rlz_tracker_chromeos.cc',
        'rlz/rlz_tracker_delegate.h',
        'rlz/rlz_tracker_ios.cc',
        'rlz/rlz_tracker_mac.cc',
        'rlz/rlz_tracker_win.cc',
      ],
      'conditions': [
        ['OS == "ios"', {
          'dependencies': [
            '../ui/base/ui_base.gyp:ui_base',
          ],
        }],
      ],
    },
  ],
}
