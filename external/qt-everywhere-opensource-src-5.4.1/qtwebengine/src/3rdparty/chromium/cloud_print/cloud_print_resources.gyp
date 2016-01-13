# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,

    'variables': {
      'version_py_path': '<(DEPTH)/build/util/version.py',
      'version_path': 'VERSION',
    },
    'version_py_path': '<(version_py_path) -f',
    'version_path': '<(version_path)',
  },
  'includes': [
    '../build/util/version.gypi',
  ],
  'targets': [
    {
      'target_name': 'cloud_print_version_resources',
      'type': 'none',
      'conditions': [
        ['branding == "Chrome"', {
          'variables': {
             'branding_path': '<(DEPTH)/chrome/app/theme/google_chrome/BRANDING',
          },
        }, { # else branding!="Chrome"
          'variables': {
             'branding_path': '<(DEPTH)/chrome/app/theme/chromium/BRANDING',
          },
        }],
      ],
      'variables': {
        'output_dir': 'cloud_print',
        'template_input_path': '../chrome/app/chrome_version.rc.version',
        'extra_variable_files_arguments': [ '-f', 'BRANDING' ],
        'extra_variable_files': [ 'BRANDING' ], # NOTE: matches that above
      },
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/<(output_dir)',
        ],
      },
      'sources': [
        'service/win/cloud_print_service_exe.ver',
        'service/win/cloud_print_service_config_exe.ver',
        'service/win/cloud_print_service_setup_exe.ver',
        'virtual_driver/win/gcp_portmon64_dll.ver',
        'virtual_driver/win/gcp_portmon_dll.ver',
        'virtual_driver/win/install/virtual_driver_setup_exe.ver',
      ],
      'includes': [
        '../chrome/version_resource_rules.gypi',
      ],
    },
    {
      'target_name': 'cloud_print_version_header',
      'type': 'none',
      'conditions': [
        ['branding == "Chrome"', {
          'variables': {
             'branding_path': '<(DEPTH)/chrome/app/theme/google_chrome/BRANDING',
          },
        }, { # else branding!="Chrome"
          'variables': {
             'branding_path': '<(DEPTH)/chrome/app/theme/chromium/BRANDING',
          },
        }],
      ],
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'version_header',
          'variables': {
            'output_dir': 'cloud_print',
            'lastchange_path':
              '<(DEPTH)/build/util/LASTCHANGE',
          },
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/<(output_dir)',
            ],
          },
          'inputs': [
            '<(version_path)',
            '<(branding_path)',
            '<(lastchange_path)',
            '<(DEPTH)/chrome/version.h.in',
            'BRANDING',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/<(output_dir)/version.h',
          ],
          'action': [
            'python',
            '<(version_py_path)',
            '-f', '<(version_path)',
            '-f', '<(branding_path)',
            '-f', '<(lastchange_path)',
            '-f', 'BRANDING',
            '<(DEPTH)/chrome/version.h.in',
            '<@(_outputs)',
          ],
          'message': 'Generating version header file: <@(_outputs)',
        },
      ],
    },
  ],
}
