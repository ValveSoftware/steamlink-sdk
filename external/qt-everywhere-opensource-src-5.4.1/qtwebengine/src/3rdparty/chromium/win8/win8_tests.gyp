# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/win_precompile.gypi',
  ],
  'targets': [
    {
      'target_name': 'test_registrar',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        # Chrome is the default viewer process currently used by the tests.
        # TODO(robertshield): Investigate building a standalone metro viewer
        # process.
        '../chrome/chrome.gyp:chrome',
        'win8.gyp:test_registrar_constants',
      ],
      'sources': [
        'test/test_registrar.cc',
        'test/test_registrar.rc',
        'test/test_registrar.rgs',
        'test/test_registrar_resource.h',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
        },
      },
    },
  ],
}
