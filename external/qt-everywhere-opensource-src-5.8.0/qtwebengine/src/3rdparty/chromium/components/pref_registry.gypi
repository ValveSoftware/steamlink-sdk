# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/pref_registry
      'target_name': 'pref_registry',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        'prefs/prefs.gyp:prefs',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'pref_registry/pref_registry_syncable.cc',
        'pref_registry/pref_registry_syncable.h',
      ],
    },
    {
      # GN version: //components/pref_registry:test_support
      'target_name': 'pref_registry_test_support',
      'type': 'static_library',
      'dependencies': [
        'pref_registry',
        'prefs/prefs.gyp:prefs_test_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'pref_registry/testing_pref_service_syncable.cc',
        'pref_registry/testing_pref_service_syncable.h',
      ],
    },
  ],
}
