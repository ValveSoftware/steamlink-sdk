# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN: //components/session_manager/core
      'target_name': 'session_manager_component',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'SESSION_IMPLEMENTATION',
      ],
      'sources': [
        'session_manager/core/session_manager.cc',
        'session_manager/core/session_manager.h',
        'session_manager/session_manager_export.h',
      ],
    },
  ],
}
