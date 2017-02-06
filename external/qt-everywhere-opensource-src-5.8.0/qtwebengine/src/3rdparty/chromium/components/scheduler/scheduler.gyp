# Copyright 2015 The Chromium Authors. All rights reserved.
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
    'scheduler.gypi',
  ],
  'targets': [
    {
      # GN version: //components/scheduler:common
      'target_name': 'scheduler_common',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        '<@(scheduler_common_sources)',
      ],
    },
    {
      # GN version: //components/scheduler:scheduler
      'target_name': 'scheduler',
      'type': '<(component)',
      'dependencies': [
        'scheduler_common',
        '../../base/base.gyp:base',
        '../../cc/cc.gyp:cc',
        '../../third_party/WebKit/public/blink.gyp:blink',
        '../../ui/gfx/gfx.gyp:gfx',
      ],
      'include_dirs': [
        '../..',
      ],
      'defines': [
        'SCHEDULER_IMPLEMENTATION',
      ],
      # Disable c4267 warnings until we fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
      'sources': [
        '<@(scheduler_sources)',
      ],
      'export_dependent_settings': [
        '../../third_party/WebKit/public/blink.gyp:blink',
      ],
    },
    {
      # GN version: //components/scheduler:test_support
      'target_name': 'scheduler_test_support',
      'type': 'static_library',
      'dependencies': [
        '../../third_party/WebKit/public/blink.gyp:blink',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        '<@(scheduler_test_support_sources)',
      ],
    },
  ],
}
