# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Include this file in a target to produce a bundled and compressed
# JavaScript file from a set of files with closure-style dependency
# declarations.  The following variables must be defined before including
# this file:
# js_root_flags: List of '-r' flags to jsbundler.py for locating the
#   .js files.
# output_file: path of the compressed JavaScript bundle.
#
# In addition, the target must have a 'sources' list containing the
# top-level files for the bundle.

{
  'actions': [
    {
      'action_name': 'js_compress',
      'message': 'Compress js for <(_target_name)',
      'variables': {
        'js_files': [
          '<!@(python tools/jsbundler.py <(js_root_flags) <(_sources))'
        ],
      },
      'inputs': [
        'tools/jsbundler.py',
        '<@(js_files)',
      ],
      'outputs': [
        '<(output_file)'
      ],
      'action': [
        'python',
        'tools/jsbundler.py',
        '-m', 'compressed_bundle',
        '-o', '<(output_file)',
        '<@(js_files)',
      ],
    },
  ],
}
