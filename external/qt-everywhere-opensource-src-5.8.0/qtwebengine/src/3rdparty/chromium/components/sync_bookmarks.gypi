# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/sync_bookmarks
      'target_name': 'sync_bookmarks',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../sync/sync.gyp:sync',
        '../ui/gfx/gfx.gyp:gfx',
        'bookmarks_browser',
        'favicon_core',
        'history_core_browser',
        'sync_driver',
        'undo_component',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'sync_bookmarks/bookmark_change_processor.cc',
        'sync_bookmarks/bookmark_change_processor.h',
        'sync_bookmarks/bookmark_data_type_controller.cc',
        'sync_bookmarks/bookmark_data_type_controller.h',
        'sync_bookmarks/bookmark_model_associator.cc',
        'sync_bookmarks/bookmark_model_associator.h',
      ],
    },
  ],
}
