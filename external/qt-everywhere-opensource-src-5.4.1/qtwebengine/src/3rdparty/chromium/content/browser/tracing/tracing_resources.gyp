# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'trace_viewer_src_dir': '../../../third_party/trace-viewer',
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/content/browser/tracing',
  },
  'targets': [
    {
      'target_name': 'generate_tracing_grd',
      'type': 'none',
      'dependencies': [
        '<(trace_viewer_src_dir)/trace_viewer.gyp:generate_about_tracing',
      ],
      'actions': [
        {
          'action_name': 'generate_tracing_grd',
          'script_name': 'generate_trace_viewer_grd.py',
          'input_pages': [
            '<(SHARED_INTERMEDIATE_DIR)/content/browser/tracing/about_tracing.html',
            '<(SHARED_INTERMEDIATE_DIR)/content/browser/tracing/about_tracing.js'
          ],
          'inputs': [
            '<@(_script_name)',
            '<@(_input_pages)'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/content/browser/tracing/tracing_resources.grd'
          ],
          'action': [
            'python', '<@(_script_name)', '<@(_input_pages)', '--output', '<@(_outputs)'
          ]
        }
      ]
    },

    {
      'target_name': 'tracing_resources',
      'type': 'none',
      'dependencies': [
        '<(trace_viewer_src_dir)/trace_viewer.gyp:generate_about_tracing',
        'generate_tracing_grd',
      ],
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/content/browser/tracing',
      },
      'actions': [
        {
          'action_name': 'tracing_resources',
          # This can't use build/grit_action.gypi because the grd file
          # is generated at build time, so the trick of using grit_info to get
          # the real inputs/outputs at GYP time isn't possible.
          'variables': {
            'grit_cmd': ['python', '../../../tools/grit/grit.py'],
            'grit_grd_file': '<(SHARED_INTERMEDIATE_DIR)/content/browser/tracing/tracing_resources.grd',
            'grit_rc_header_format%': '',
          },
          'inputs': [
            '<(grit_grd_file)',
            '<!@pymod_do_main(grit_info --inputs)',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/tracing_resources.h',
            '<(grit_out_dir)/tracing_resources.pak',
          ],
          'action': ['<@(grit_cmd)',
                     '-i', '<(grit_grd_file)', 'build',
                     '-f', '<(DEPTH)/tools/gritsettings/resource_ids',
                     '-o', '<(grit_out_dir)',
                     '-D', 'SHARED_INTERMEDIATE_DIR=<(SHARED_INTERMEDIATE_DIR)',
                     '<@(grit_defines)',
                     '<@(grit_rc_header_format)'],
          'message': 'Generating resources from <(grit_grd_file)',
        }
      ],
      'includes': [ '../../../build/grit_target.gypi' ]
    }
  ]
}
