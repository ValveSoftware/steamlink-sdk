# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'version_py': '<(DEPTH)/build/util/version.py',
    'version_path': '../../chrome/VERSION',
    'lastchange_path': '<(DEPTH)/build/util/LASTCHANGE',
    'branding_dir': '../app/theme/<(branding_path_component)',
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
      {
        'target_name': 'mini_installer_version',
        'type': 'none',
        'sources': [
         'mini_installer/mini_installer_exe_version.rc.version',
        ],
        'rules': [
          {
            'rule_name': 'mini_installer_version',
            'extension': 'version',
            'variables': {
              'template_input_path': 'mini_installer/mini_installer_exe_version.rc.version',
            },
            'inputs': [
              '<(template_input_path)',
              '<(version_path)',
              '<(lastchange_path)',
              '<(branding_dir)/BRANDING',
            ],
            'outputs': [
              '<(PRODUCT_DIR)/mini_installer_exe_version.rc',
            ],
            'action': [
              'python', '<(version_py)',
              '-f', '<(version_path)',
              '-f', '<(lastchange_path)',
              '-f', '<(branding_dir)/BRANDING',
              '<(template_input_path)',
              '<@(_outputs)',
            ],
            'process_outputs_as_sources': 1,
            'message': 'Generating version information'
          },
        ],
      }],
    }],
  ],
}
