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
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',
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
                     '-D', 'SHARED_INTERMEDIATE_DIR=<(SHARED_INTERMEDIATE_DIR)',
                     '<@(grit_defines)',
                     '<@(grit_rc_header_format)'],
          'message': 'Generating resources from <(grit_grd_file)',
        },
        {
          'action_name': 'devtools_protocol_constants',
          'variables': {
            'blink_protocol': '../../../third_party/WebKit/Source/devtools/protocol.json',
            'browser_protocol': 'browser_protocol.json',
            'generator': '../../public/browser/devtools_protocol_constants_generator.py',
            'package': 'content'
          },
          'inputs': [
            '<(blink_protocol)',
            '<(browser_protocol)',
            '<(generator)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/<(package)/browser/devtools/devtools_protocol_constants.cc',
            '<(SHARED_INTERMEDIATE_DIR)/<(package)/browser/devtools/devtools_protocol_constants.h'
          ],
          'action':[
            'python',
            '<(generator)',
            '<(package)',
            '<(SHARED_INTERMEDIATE_DIR)/<(package)/browser/devtools/devtools_protocol_constants.cc',
            '<(SHARED_INTERMEDIATE_DIR)/<(package)/browser/devtools/devtools_protocol_constants.h',
            '<(blink_protocol)',
            '<(browser_protocol)',
          ],
          'message': 'Generating DevTools protocol constants from <(blink_protocol)'
        }
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
