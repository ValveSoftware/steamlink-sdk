# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'prefs',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'include_dirs': [
        '../..',
      ],
      'defines': [
        'COMPONENTS_PREFS_IMPLEMENTATION',
      ],
      'sources': [
        'default_pref_store.cc',
        'default_pref_store.h',
        'in_memory_pref_store.cc',
        'in_memory_pref_store.h',
        'json_pref_store.cc',
        'json_pref_store.h',
        'overlay_user_pref_store.cc',
        'overlay_user_pref_store.h',
        'pref_change_registrar.cc',
        'pref_change_registrar.h',
        'pref_member.cc',
        'pref_member.h',
        'pref_notifier_impl.cc',
        'pref_notifier_impl.h',
        'pref_registry.cc',
        'pref_registry.h',
        'pref_registry_simple.cc',
        'pref_registry_simple.h',
        'pref_service.cc',
        'pref_service.h',
        'pref_service_factory.cc',
        'pref_service_factory.h',
        'pref_store.cc',
        'pref_store.h',
        'pref_value_map.cc',
        'pref_value_map.h',
        'pref_value_store.cc',
        'pref_value_store.h',
        'scoped_user_pref_update.cc',
        'scoped_user_pref_update.h',
        'value_map_pref_store.cc',
        'value_map_pref_store.h',
      ],
      'conditions': [
        ['OS!="ios"', {
          'sources': [
            'base_prefs_export.h',
            'persistent_pref_store.h',
            'pref_filter.h',
            'pref_notifier.h',
            'pref_observer.h',
            'writeable_pref_store.h',
          ]
        }]
      ],
    },
    {
      'target_name': 'prefs_test_support',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        'prefs',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'sources': [
        'mock_pref_change_callback.cc',
        'mock_pref_change_callback.h',
        'pref_store_observer_mock.cc',
        'pref_store_observer_mock.h',
        'testing_pref_service.cc',
        'testing_pref_service.h',
        'testing_pref_store.cc',
        'testing_pref_store.h',
      ],
    },
  ],
}
