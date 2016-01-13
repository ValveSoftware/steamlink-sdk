# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'undoview',
      'type': 'static_library',
      'sources': [
        'undo_manager.h',
        'undo_manager.c',
        'undo_view.h',
        'undo_view.c',
      ],

      'dependencies' : [
        '../../build/linux/system.gyp:gtk',
      ],
    },
  ],
}
