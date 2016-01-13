# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'bookmarks_browser',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../third_party/icu/icu.gyp:icuuc',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../url/url.gyp:url_lib',
        'bookmarks_common',
        'components_strings.gyp:components_strings',
        'favicon_base',
        'keyed_service_core',
        'pref_registry',
        'query_parser',
        'startup_metric_utils',
      ],
      'sources': [
        'bookmarks/browser/base_bookmark_model_observer.cc',
        'bookmarks/browser/base_bookmark_model_observer.h',
        'bookmarks/browser/bookmark_client.cc',
        'bookmarks/browser/bookmark_client.h',
        'bookmarks/browser/bookmark_codec.cc',
        'bookmarks/browser/bookmark_codec.h',
        'bookmarks/browser/bookmark_expanded_state_tracker.cc',
        'bookmarks/browser/bookmark_expanded_state_tracker.h',
        'bookmarks/browser/bookmark_index.cc',
        'bookmarks/browser/bookmark_index.h',
        'bookmarks/browser/bookmark_match.cc',
        'bookmarks/browser/bookmark_match.h',
        'bookmarks/browser/bookmark_model.cc',
        'bookmarks/browser/bookmark_model.h',
        'bookmarks/browser/bookmark_model_observer.h',
        'bookmarks/browser/bookmark_node.cc',
        'bookmarks/browser/bookmark_node.h',
        'bookmarks/browser/bookmark_node_data.cc',
        'bookmarks/browser/bookmark_node_data.h',
        'bookmarks/browser/bookmark_node_data_ios.cc',
        'bookmarks/browser/bookmark_node_data_mac.cc',
        'bookmarks/browser/bookmark_node_data_views.cc',
        'bookmarks/browser/bookmark_pasteboard_helper_mac.h',
        'bookmarks/browser/bookmark_pasteboard_helper_mac.mm',
        'bookmarks/browser/bookmark_storage.cc',
        'bookmarks/browser/bookmark_storage.h',
        'bookmarks/browser/bookmark_utils.cc',
        'bookmarks/browser/bookmark_utils.h',
        'bookmarks/browser/scoped_group_bookmark_actions.cc',
        'bookmarks/browser/scoped_group_bookmark_actions.h',
      ],
    },
    {
      'target_name': 'bookmarks_common',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'bookmarks/common/bookmark_constants.cc',
        'bookmarks/common/bookmark_constants.h',
        'bookmarks/common/bookmark_pref_names.cc',
        'bookmarks/common/bookmark_pref_names.h',
      ],
    },
    {
      'target_name': 'bookmarks_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gmock.gyp:gmock',
        '../ui/events/platform/events_platform.gyp:events_platform',
        '../url/url.gyp:url_lib',
        'bookmarks_browser',
      ],
      'sources': [
        'bookmarks/test/bookmark_test_helpers.cc',
        'bookmarks/test/bookmark_test_helpers.h',
        'bookmarks/test/mock_bookmark_model_observer.cc',
        'bookmarks/test/mock_bookmark_model_observer.h',
        'bookmarks/test/test_bookmark_client.cc',
        'bookmarks/test/test_bookmark_client.h',
      ],
      'conditions': [
        ['use_x11==1', {
          'dependencies': [
            '../ui/events/platform/x11/x11_events_platform.gyp:x11_events_platform',
          ],
        }],
      ],
    },
  ],
}
