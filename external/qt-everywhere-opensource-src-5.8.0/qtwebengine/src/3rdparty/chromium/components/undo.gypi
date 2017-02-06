# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/undo
      'target_name': 'undo_component',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/base/ui_base.gyp:ui_base',
        'bookmarks_browser',
        'components_strings.gyp:components_strings',
        'keyed_service_core',
      ],
      'sources': [
        'undo/bookmark_undo_service.cc',
        'undo/bookmark_undo_service.h',
        'undo/bookmark_undo_utils.cc',
        'undo/bookmark_undo_utils.h',
        'undo/undo_manager.cc',
        'undo/undo_manager.h',
        'undo/undo_manager_observer.h',
        'undo/undo_operation.h',
      ],
    },
  ],
}
