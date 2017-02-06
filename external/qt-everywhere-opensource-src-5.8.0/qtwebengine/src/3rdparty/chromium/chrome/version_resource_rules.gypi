# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file defines rules that allow you to generate version resources for
# Windows images.
#
# GN: Use the "process_version" template in //chrome/version.gni.
# For an example, see the target //chrome:chrome_exe_version

# Include 'version.gypi' at the top of your GYP file to define
# the required variables:
#
#  'includes': [
#    '<(DEPTH)/build/util/version.gypi',
#  ],
#
# Then include this rule file in a productname_resources target:
#
#    {
#      'target_name': 'chrome_version_resources',
#      ...
#      'variables': {
#        'output_dir': 'product_version',
#        'branding_path': 'some/branding/file',
#        'template_input_path': 'some/product_version.rc.version',
#        'extra_variable_files_arguments': [ '-f', 'some/file/with/variables' ],
#        'extra_variable_files': [ 'some/file/with/variables' ], # NOTE: matches that above
#      },
#      'includes': [
#        '<(DEPTH)/chrome/version_resource_rules.gypi',
#      ],
#    }
#
{
  'rules': [
    {
      'rule_name': 'version',
      'extension': 'ver',
      'variables': {
        'lastchange_path': '<(DEPTH)/build/util/LASTCHANGE',
	'extra_variable_files%': [],
        'extra_variable_files_arguments%': [],
      },
      'inputs': [
        '<(version_py_path)',
        '<(version_path)',
        '<(branding_path)',
        '<(lastchange_path)',
        '<@(extra_variable_files)',
        '<(template_input_path)',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(output_dir)/<(RULE_INPUT_ROOT)_version.rc',
      ],
      'action': [
        'python',
        '<(version_py_path)',
        '-f', '<(RULE_INPUT_PATH)',
        '-f', '<(version_path)',
        '-f', '<(branding_path)',
        '-f', '<(lastchange_path)',
        '<@(extra_variable_files_arguments)',
        '<(template_input_path)',
        '<@(_outputs)',
      ],
      'message': 'Generating version information in <@(_outputs)'
    },
  ],
}
