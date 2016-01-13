# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'policy_path_parser',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../chrome/common_constants.gyp:common_constants',
        '../components/components.gyp:policy',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'browser/policy/policy_path_parser.h',
        'browser/policy/policy_path_parser_linux.cc',
        'browser/policy/policy_path_parser_mac.mm',
        'browser/policy/policy_path_parser_win.cc',
      ],
    },
  ],
}
