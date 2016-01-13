# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This file is meant to be included into a target to provide a rule
# to run the equivalent of jarjar on Java resources (layout.xml files).
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my-package_java',
#   'type': 'none',
#   'variables': {
#     'java_in_dir': 'path/to/package/root',
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
#
# Required variables:
#  res_dir - The top-level resources folder.
#  rules_file - Path to the file containing jar-jar rules.

{
  'variables': {
    'intermediate_dir': '<(SHARED_INTERMEDIATE_DIR)/<(_target_name)',
    'jarjar_stamp': '<(intermediate_dir)/jarjar_resources.stamp',
    'resource_input_paths': ['<!@(find <(res_dir) -type f)'],
  },
  'actions': [{
      'action_name': 'jarjar resources',
      'message': 'Copying and jar-jaring resources for <(_target_name)',
      'variables': {
        'out_dir': '<(intermediate_dir)/jarjar_res',
      },
      'inputs': [
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/jarjar_resources.py',
        '>@(resource_input_paths)',
      ],
      'outputs': [
          '<(jarjar_stamp)',
      ],
      'action': [
        'python', '../build/android/gyp/jarjar_resources.py',
        '--input-dir', '<(res_dir)',
        '--output-dir', '<(out_dir)',
        '--rules-path', '<(rules_file)',
        '--stamp', '<(jarjar_stamp)',
      ]
  }],
}
