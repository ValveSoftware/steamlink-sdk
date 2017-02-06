# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/bookmarks/browser
      'target_name': 'bookmarks_browser',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../net/net.gyp:net',
        '../third_party/icu/icu.gyp:icui18n',
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
        'url_formatter/url_formatter.gyp:url_formatter',
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
        'bookmarks/browser/bookmark_undo_delegate.h',
        'bookmarks/browser/bookmark_undo_provider.h',
        'bookmarks/browser/bookmark_utils.cc',
        'bookmarks/browser/bookmark_utils.h',
        'bookmarks/browser/scoped_group_bookmark_actions.cc',
        'bookmarks/browser/scoped_group_bookmark_actions.h',
        'bookmarks/browser/startup_task_runner_service.cc',
        'bookmarks/browser/startup_task_runner_service.h',
      ],
      'conditions': [
        ['OS == "android"', {
          # In GN, this android-specific stuff is its own target at
          # //components/bookmarks/common/android
          # TODO(cjhopman): This should be its own target in Gyp, too.
          'dependencies': [
            'bookmarks_jni_headers',
          ],
          'sources' : [
            'bookmarks/common/android/bookmark_id.cc',
            'bookmarks/common/android/bookmark_id.h',
            'bookmarks/common/android/bookmark_type_list.h',
            'bookmarks/common/android/component_jni_registrar.cc',
            'bookmarks/common/android/component_jni_registrar.h',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '<(DEPTH)/ui/views/views.gyp:views',
          ],
        }],
        ['use_x11==1', {
          'dependencies': [
            '../ui/events/platform/x11/x11_events_platform.gyp:x11_events_platform',
            '../ui/gfx/x/gfx_x11.gyp:gfx_x11',
          ],
        }],
      ],
    },
    {
      # GN version: //components/bookmarks/common
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
      # GN version: //components/bookmarks/managed
      'target_name': 'bookmarks_managed',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'bookmarks_browser',
        'components_strings.gyp:components_strings',
        'keyed_service_core',
      ],
      'sources': [
        'bookmarks/managed/managed_bookmark_service.cc',
        'bookmarks/managed/managed_bookmark_service.h',
        'bookmarks/managed/managed_bookmark_util.cc',
        'bookmarks/managed/managed_bookmark_util.h',
        'bookmarks/managed/managed_bookmarks_tracker.cc',
        'bookmarks/managed/managed_bookmarks_tracker.h',
      ],
    },
    {
      # GN version: //components/bookmarks/test
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
  'conditions' : [
    ['OS=="android"', {
      'targets': [
        {
          # GN: //components/bookmarks/common/android:bookmarks_java
          'target_name': 'bookmarks_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java',
            'bookmark_type_java',
          ],
          'variables': {
            'java_in_dir': 'bookmarks/common/android/java',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          # GN: //components/bookmarks/common/android:bookmarks_jni_headers
          'target_name': 'bookmarks_jni_headers',
          'type': 'none',
          'sources': [
            'bookmarks/common/android/java/src/org/chromium/components/bookmarks/BookmarkId.java',
          ],
          'variables': {
            'jni_gen_package': 'components/bookmarks',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          # GN: //components/bookmarks/common/android:bookmarks_type_javagen
          'target_name': 'bookmark_type_java',
          'type': 'none',
          'variables': {
            'source_file': 'bookmarks/common/android/bookmark_type.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
      ],
    }]
  ],
}
