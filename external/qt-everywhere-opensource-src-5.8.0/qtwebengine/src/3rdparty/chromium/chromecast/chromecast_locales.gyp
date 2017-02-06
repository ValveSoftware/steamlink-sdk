# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromecast_branding%': 'public',
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chromecast_strings',
  },
  'targets': [
    {
      'target_name': 'chromecast_settings',
      'type': 'none',
      'actions': [
        {
          'action_name': 'chromecast_settings',
          'variables': {
            'grit_grd_file': 'app/resources/chromecast_settings.grd',
            'grit_resource_ids': 'app/resources/resource_ids',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'chromecast_locales_pak',
      'type': 'none',
      'dependencies': [
        'chromecast_settings',
      ],
      'conditions': [
        ['chromecast_branding!="public"', {
          'dependencies': [
            'internal/chromecast_locales.gyp:chromecast_app_strings',
          ],
        }],
      ],
      'actions': [
        {
          'action_name': 'repack_locales',
          'message': 'Packing locale-specific resources',
          'variables': {
            'repack_python_cmd': '<(DEPTH)/chromecast/tools/build/chromecast_repack_locales.py',
            'repack_output_dir': '<(PRODUCT_DIR)/chromecast_locales',
            'repack_locales_cmd': [
              'python',
              '<(repack_python_cmd)'
            ],
          },
          'inputs': [
            '<(repack_python_cmd)',
            '<!@pymod_do_main(chromecast_repack_locales -i -b <(chromecast_branding) -g <(grit_out_dir) -x <(repack_output_dir) <(locales))'
          ],
          'outputs': [
            '<!@pymod_do_main(chromecast_repack_locales -o -b <(chromecast_branding) -g <(grit_out_dir) -x <(repack_output_dir) <(locales))'
          ],
          'action': [
            '<@(repack_locales_cmd)',
            '-b', '<(chromecast_branding)',
            '-g', '<(grit_out_dir)',
            '-x', '<(repack_output_dir)/.',
            '<@(locales)',
          ],
        },
      ],
    },
  ],
}
