# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'devtools_resources',
      'type': 'none',
      'dependencies': [
        '../../../third_party/WebKit/public/blink_devtools.gyp:blink_generate_devtools_grd',
      ],
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/blink',
      },
      'actions': [
        {
          'action_name': 'devtools_resources',
          # This can't use build/grit_action.gypi because the grd file
          # is generated at build time, so the trick of using grit_info to get
          # the real inputs/outputs at GYP time isn't possible.
          'variables': {
            'grit_cmd': ['python', '../../../tools/grit/grit.py'],
            'grit_grd_file': '<(SHARED_INTERMEDIATE_DIR)/devtools/devtools_resources.grd',
            'grit_rc_header_format%': '',

            'conditions': [
              # These scripts can skip writing generated files if they are
              # identical to the already existing files, which avoids further
              # build steps, like recompilation. However, a dependency (earlier
              # build step) having a newer timestamp than an output (later
              # build step) confuses some build systems, so only use this on
              # ninja, which explicitly supports this use case (gyp turns all
              # actions into ninja restat rules).
              ['"<(GENERATOR)"=="ninja"', {
                'write_only_new': '1',
              }, {
                'write_only_new': '0',
              }],
            ],
          },
          'inputs': [
            '<(grit_grd_file)',
            '<!@pymod_do_main(grit_info --inputs)',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/devtools_resources.h',
            '<(grit_out_dir)/devtools_resources.pak',
            '<(grit_out_dir)/grit/devtools_resources_map.cc',
            '<(grit_out_dir)/grit/devtools_resources_map.h',
          ],
          'action': ['<@(grit_cmd)',
                     '-i', '<(grit_grd_file)', 'build',
                     '-f', '<(DEPTH)/tools/gritsettings/resource_ids',
                     '-o', '<(grit_out_dir)',
                     '--write-only-new=<(write_only_new)',
                     '-D', 'SHARED_INTERMEDIATE_DIR=<(SHARED_INTERMEDIATE_DIR)',
                     '<@(grit_defines)',
                     '<@(grit_rc_header_format)'],
          'message': 'Generating resources from <(grit_grd_file)',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ]
      },
      'includes': [ '../../../build/grit_target.gypi' ],
    },
  ],
}
