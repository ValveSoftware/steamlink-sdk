# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/handoff
      'target_name': 'handoff',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['OS=="mac" or OS=="ios"', {
          'sources': [
            'handoff/handoff_utility.h',
            'handoff/handoff_utility.mm',
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            'handoff/handoff_manager.h',
            'handoff/handoff_manager.mm',
          ],
          'dependencies': [
            '../base/base.gyp:base',
          ],
        }],
      ],
    },
  ],
}
