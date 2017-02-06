# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'user_prefs',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        'prefs/prefs.gyp:prefs',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'USER_PREFS_IMPLEMENTATION',
      ],
      'sources': [
        'user_prefs/user_prefs.cc',
        'user_prefs/user_prefs.h',
        'user_prefs/user_prefs_export.h',
      ],
    },
    {
      'target_name': 'user_prefs_tracked',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
      ],
      'sources': [
        'user_prefs/tracked/device_id.h',
        'user_prefs/tracked/device_id_mac.cc',
        'user_prefs/tracked/device_id_stub.cc',
        'user_prefs/tracked/device_id_win.cc',
        'user_prefs/tracked/dictionary_hash_store_contents.cc',
        'user_prefs/tracked/dictionary_hash_store_contents.h',
        'user_prefs/tracked/hash_store_contents.h',
        'user_prefs/tracked/interceptable_pref_filter.cc',
        'user_prefs/tracked/interceptable_pref_filter.h',
        'user_prefs/tracked/pref_hash_calculator.cc',
        'user_prefs/tracked/pref_hash_calculator.h',
        'user_prefs/tracked/pref_hash_filter.cc',
        'user_prefs/tracked/pref_hash_filter.h',
        'user_prefs/tracked/pref_hash_store.h',
        'user_prefs/tracked/pref_hash_store_impl.cc',
        'user_prefs/tracked/pref_hash_store_impl.h',
        'user_prefs/tracked/pref_hash_store_transaction.h',
        'user_prefs/tracked/pref_names.cc',
        'user_prefs/tracked/pref_names.h',
        'user_prefs/tracked/pref_service_hash_store_contents.cc',
        'user_prefs/tracked/pref_service_hash_store_contents.h',
        'user_prefs/tracked/segregated_pref_store.cc',
        'user_prefs/tracked/segregated_pref_store.h',
        'user_prefs/tracked/tracked_atomic_preference.cc',
        'user_prefs/tracked/tracked_atomic_preference.h',
        'user_prefs/tracked/tracked_preference.h',
        'user_prefs/tracked/tracked_preference_helper.cc',
        'user_prefs/tracked/tracked_preference_helper.h',
        'user_prefs/tracked/tracked_preference_histogram_names.cc',
        'user_prefs/tracked/tracked_preference_histogram_names.h',
        'user_prefs/tracked/tracked_preference_validation_delegate.h',
        'user_prefs/tracked/tracked_preferences_migration.cc',
        'user_prefs/tracked/tracked_preferences_migration.h',
        'user_prefs/tracked/tracked_split_preference.cc',
        'user_prefs/tracked/tracked_split_preference.h',
      ],
      'conditions': [
        ['OS=="win" or (OS=="mac" and OS!="ios")', {
          'sources!': [
            'user_prefs/tracked/device_id_stub.cc',
          ],
        }],
        ['OS=="ios"', {
          'sources!': [
            'user_prefs/tracked/device_id_mac.cc',
          ],
        }],
      ],

      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      'target_name': 'user_prefs_tracked_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
      ],
      'sources': [
        'user_prefs/tracked/mock_validation_delegate.cc',
        'user_prefs/tracked/mock_validation_delegate.h',
      ],
    },
  ],
}
