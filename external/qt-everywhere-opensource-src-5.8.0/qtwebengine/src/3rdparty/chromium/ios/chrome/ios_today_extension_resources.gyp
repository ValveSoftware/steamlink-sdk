# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'grit_base_dir': '<(SHARED_INTERMEDIATE_DIR)',
    'grit_out_dir': '<(grit_base_dir)/ios/today_extension',
  },
  'targets': [
    {
      # GN version: //ios/chrome/today_extension:resources
      'target_name': 'ios_today_extension_resources',
      'type': 'none',
      'dependencies': [
        'ios_today_extension_strings_gen',
      ],
    },
    {
      # GN version: //ios/chrome/today_extension/strings
      'target_name': 'ios_today_extension_strings_gen',
      'type': 'none',
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'generate_ios_today_extension_strings',
          'variables': {
            'grit_grd_file': 'today_extension/strings/ios_today_extension_strings.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
      # Override the exported include-dirs; ios/chrome/grit/ios_*strings.h
      # should only be referenceable as ios/chrome/grit to allow DEPS-time
      # checking of usage.
      'direct_dependent_settings': {
        'include_dirs': [
          '<(grit_base_dir)',
        ],
        'include_dirs!': [
          '<(grit_out_dir)',
        ],
      }
    },
    {
      # GN version: //ios/chrome/today_extension:packed_resources
      'target_name': 'ios_today_extension_packed_resources',
      'type': 'none',
      'dependencies': [
        'ios_today_extension_resources',
      ],
      'actions': [
        {
          'action_name': 'repack_ios_today_extension_locales',
          'variables': {
            'repack_locales_path': 'tools/build/ios_repack_extension_locales.py',
          },
          'inputs': [
            'tools/build/ios_repack_extension_locales.py',
            '<!@pymod_do_main(ios_repack_extension_locales -i '
              '-n today_extension '
              '-s <(SHARED_INTERMEDIATE_DIR) '
              '-x <(SHARED_INTERMEDIATE_DIR)/repack_today_extension '
              '-b <(branding_path_component) '
              '<(locales))'
          ],
          'outputs': [
            '<!@pymod_do_main(ios_repack_extension_locales -o '
              '-n today_extension '
              '-s <(SHARED_INTERMEDIATE_DIR) '
              '-x <(SHARED_INTERMEDIATE_DIR)/repack_today_extension '
              '<(locales))'
          ],
          'action': [
            'python',
            'tools/build/ios_repack_extension_locales.py',
            '-n', 'today_extension',
            '-x', '<(SHARED_INTERMEDIATE_DIR)/repack_today_extension',
            '-s', '<(SHARED_INTERMEDIATE_DIR)',
            '-b', '<(branding_path_component)',
            '<@(locales)',
          ],
        },
      ],
    },
  ],
}

