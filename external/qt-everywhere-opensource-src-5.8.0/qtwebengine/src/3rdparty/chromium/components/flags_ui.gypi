# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/flags_ui
      'target_name': 'flags_ui',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../components/components_strings.gyp:components_strings',
        '../components/prefs/prefs.gyp:prefs',
        '../ui/base/ui_base.gyp:ui_base',
        'flags_ui_switches',
        'pref_registry',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'flags_ui/feature_entry.cc',
        'flags_ui/feature_entry.h',
        'flags_ui/feature_entry_macros.h',
        'flags_ui/flags_state.cc',
        'flags_ui/flags_state.h',
        'flags_ui/flags_storage.h',
        'flags_ui/flags_ui_constants.cc',
        'flags_ui/flags_ui_constants.h',
        'flags_ui/flags_ui_pref_names.cc',
        'flags_ui/flags_ui_pref_names.h',
        'flags_ui/pref_service_flags_storage.cc',
        'flags_ui/pref_service_flags_storage.h',
      ],
    },
    {
      # GN version: //components/flags_ui:switches
      # This is a separate target so that the dependencies of
      # //chrome/common can be kept minimal.
      'target_name': 'flags_ui_switches',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'flags_ui/flags_ui_switches.cc',
        'flags_ui/flags_ui_switches.h',
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          # The flags_ui_switches_win64 target here allows us to use base for
          # Win64 targets (the normal build is 32 bits).
          'target_name': 'flags_ui_switches_win64',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'sources': [
            # Note: sources list duplicated in GN build.
            'flags_ui/flags_ui_switches.cc',
            'flags_ui/flags_ui_switches.h',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}
