# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'pack_policy_templates',
      'type': 'none',
      'conditions': [
        ['OS=="win" or OS=="mac" or OS=="linux"', {
          'dependencies': [
            '../components/components.gyp:policy_templates',
          ],
        }],
        ['OS=="win"', {
          'variables': {
            'version_path': '<(grit_out_dir)/app/policy/VERSION',
          },
          'actions': [
            {
              'action_name': 'add_version',
              'inputs': [
                'VERSION',
              ],
              'outputs': [
                '<(version_path)',
              ],
              'action': [
                'python',
                '../build/cp.py',
                '<@(_inputs)',
                '<@(_outputs)',
              ],
            },
            {
              # Add all the templates generated at the previous step into
              # a zip archive.
              'action_name': 'pack_templates',
              'variables': {
                'grit_grd_file': '../components/policy/resources/policy_templates.grd',
                'grit_info_cmd': [
                  'python',
                  '<(DEPTH)/tools/grit/grit_info.py',
                  '<@(grit_defines)',
                ],
                'template_files': [
                  '<!@(<(grit_info_cmd) --outputs \'<(grit_out_dir)\' <(grit_grd_file))',
                ],
                'zip_script': '../components/policy/tools/make_policy_zip.py',
              },
              'inputs': [
                '<(version_path)',
                '<@(template_files)',
                '<(zip_script)',
                '<!@pymod_do_main(grit_info <@(grit_defines) '
                    '--inputs "<(grit_grd_file)" '
                    '-f "<(DEPTH)/tools/gritsettings/resource_ids")',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/policy_templates.zip',
              ],
              'action': [
                'python',
                '<(zip_script)',
                '--output',
                '<@(_outputs)',
                '--basedir', '<(grit_out_dir)/app/policy',
                # The list of files in the destination zip is derived from
                # the list of output nodes in the following grd file.
                # This whole trickery is necessary because we cannot pass
                # the entire list of file names as command line arguments,
                # because they would exceed the length limit on Windows.
                '--grd_input',
                '<(grit_grd_file)',
                '--grd_strip_path_prefix',
                'app/policy',
                '--extra_input',
                'VERSION',
                # Module to be used to process grd_input'.
                '--grit_info',
                '<(DEPTH)/tools/grit/grit_info.py',
                '<@(grit_defines)',
              ],
              'message': 'Packing generated templates into <(_outputs)',
            },
          ],
        }],
      ],
    },
  ],
}
