# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Include this file in a target to copy all necessary files to satisfy
# the transitive dependencies of a set of top-level JavaScript files
# using closure-style dependency declarations.  The following variables
# must be available when this file is included:
# js_root_flags: List of '-r' flags to jsbundler.py for locating the
#   .js files.
# path_rewrite_flags: '-w' flags to jsbundler.py to replace source path
#   prefixes at the destination.
# dest_dir: Destination directory for all copied files.

{
  'actions': [
    {
      'action_name': 'copy_js',
      'message': 'Copy JS for <(_target_name)',
      'variables': {
        'js_bundler_path': 'tools/jsbundler.py',
        'js_files': [
          '<!@(python <(js_bundler_path) <(js_root_flags) <(_sources))'
        ],
      },
      'inputs': [
        '<(js_bundler_path)',
        '<@(js_files)',
      ],
      'outputs': [
        '<!@(python <(js_bundler_path) <@(path_rewrite_flags) -d <(dest_dir) <@(js_files))'
      ],
      'action': [
        'python',
        '<(js_bundler_path)',
        '-m', 'copy',
        '-d', '<(dest_dir)',
        '<@(path_rewrite_flags)',
        '<@(js_files)',
      ],
    },
  ]
}
