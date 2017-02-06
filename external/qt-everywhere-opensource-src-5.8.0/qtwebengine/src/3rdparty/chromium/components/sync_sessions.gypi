# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/sync_sessions
      'target_name': 'sync_sessions',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../sync/sync.gyp:sync',
        '../url/url.gyp:url_lib',
        'bookmarks_browser',
        'history_core_browser',
        'prefs/prefs.gyp:prefs',
        'sync_driver',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'sync_sessions/favicon_cache.cc',
        'sync_sessions/favicon_cache.h',
        'sync_sessions/local_session_event_router.h',
        'sync_sessions/open_tabs_ui_delegate.cc',
        'sync_sessions/open_tabs_ui_delegate.h',
        'sync_sessions/revisit/bookmarks_by_url_provider_impl.cc',
        'sync_sessions/revisit/bookmarks_by_url_provider_impl.h',
        'sync_sessions/revisit/bookmarks_page_revisit_observer.cc',
        'sync_sessions/revisit/bookmarks_page_revisit_observer.h',
        'sync_sessions/revisit/current_tab_matcher.cc',
        'sync_sessions/revisit/current_tab_matcher.h',
        'sync_sessions/revisit/offset_tab_matcher.cc',
        'sync_sessions/revisit/offset_tab_matcher.h',
        'sync_sessions/revisit/page_equality.h',
        'sync_sessions/revisit/page_visit_observer.h',
        'sync_sessions/revisit/page_revisit_broadcaster.cc',
        'sync_sessions/revisit/page_revisit_broadcaster.h',
        'sync_sessions/revisit/sessions_page_revisit_observer.cc',
        'sync_sessions/revisit/sessions_page_revisit_observer.h',
        'sync_sessions/revisit/typed_url_page_revisit_observer.cc',
        'sync_sessions/revisit/typed_url_page_revisit_observer.h',
        'sync_sessions/revisit/typed_url_page_revisit_task.cc',
        'sync_sessions/revisit/typed_url_page_revisit_task.h',
        'sync_sessions/session_data_type_controller.cc',
        'sync_sessions/session_data_type_controller.h',
        'sync_sessions/sessions_sync_manager.cc',
        'sync_sessions/sessions_sync_manager.h',
        'sync_sessions/synced_tab_delegate.cc',
        'sync_sessions/synced_tab_delegate.h',
        'sync_sessions/synced_session.cc',
        'sync_sessions/synced_session.h',
        'sync_sessions/synced_session_tracker.cc',
        'sync_sessions/synced_session_tracker.h',
        'sync_sessions/synced_window_delegate.h',
        'sync_sessions/synced_window_delegates_getter.cc',
        'sync_sessions/synced_window_delegates_getter.h',
        'sync_sessions/sync_sessions_client.cc',
        'sync_sessions/sync_sessions_client.h',
        'sync_sessions/sync_sessions_metrics.cc',
        'sync_sessions/sync_sessions_metrics.h',
        'sync_sessions/tab_node_pool.cc',
        'sync_sessions/tab_node_pool.h',
      ],
      'conditions': [
        ['OS!="ios"', {
          'dependencies': [
            'sessions_content',
          ],
        }, {  # OS==ios
          'dependencies': [
            'sessions_ios',
          ],
        }],
      ],
    },
    {
      'target_name': 'sync_sessions_test_support',
      'type': 'static_library',
      'dependencies': [
        'sync_sessions',
        '../base/base.gyp:base',
        '../sync/sync.gyp:sync',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'sync_sessions/fake_sync_sessions_client.cc',
        'sync_sessions/fake_sync_sessions_client.h',
      ],
    }
  ]
}
