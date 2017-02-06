# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Include this file in a target to generate a Closure style deps.js file.
#
# The following variables must be available when this file is included:
# js_root_flags: List of '-r' flags to jsbundler.py for locating the
#   .js files.
# deps_js_output_file: Where to write the generated deps file.

{
  'includes': ['common.gypi'],
  'actions': [
    {
      'action_name': 'generate_deps',
      'message': 'Generate deps for <(_target_name)',
      'variables': {
        'js_bundler_path': 'tools/jsbundler.py',
        'closure_depswriter_path': 'tools/generate_deps.py',
        'js_files': [
          '<!@(python <(js_bundler_path) <(js_root_flags) <(_sources))'
        ],
      },
      'inputs': [
        '<(js_bundler_path)',
        '<(closure_depswriter_path)',
        '<@(js_files)',
      ],
      'outputs': [
        '<(deps_js_output_file)',
      ],
      'action': [
        'python',
        '<(closure_depswriter_path)',
        '-w', '<(closure_goog_dir):../closure/',
        '-w', ':../',
        '--output_file', '<(deps_js_output_file)',
        '<@(js_files)',
      ],
    },
  ]
}
