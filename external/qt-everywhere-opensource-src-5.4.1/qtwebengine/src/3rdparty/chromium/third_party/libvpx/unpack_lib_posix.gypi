# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This action takes an archive (.a) file and unpacks it unto object (.o) files.
# The following input gyp variables are required:
#   unpack_lib_output_dir, the output directory of extracted object file
#   unpack_lib_name, the object file to be extracted.
#   unpack_lib_search_path_list, a list of paths to search for the library.
#   it must be ['-a', 'path_name1', '-a', 'path_name2'...]
#
# For example:
#   'variables': {
#     'unpack_lib_search_path_list': [
#       '-a', '/a/lib.a',
#       '-a', 'b/lib.a',
#     ],
#     'unpack_lib_output_dir':'ouput',
#     'unpack_lib_name':'offsets.o'
#   },
#   'includes': ['unpack_lib_posix.gypi'],
#
# It unpacks the first existing library in 'unpack_lib_search_path_list', and
# extracts 'offsets.o' to 'output' directory.

{
  'actions': [
    {
      'variables' : {
        'ar_cmd': [],
	'conditions': [
          ['android_webview_build==1', {
            'ar_cmd': ['-r', '$(abspath $($(gyp_var_prefix)TARGET_AR))'],
          }],
        ],
      },
      'action_name': 'unpack_lib_posix',
      'inputs': [
        'unpack_lib_posix.sh',
      ],
      'outputs': [
        '<(unpack_lib_output_dir)/<(unpack_lib_name)',
      ],
      'action': [
        '<(DEPTH)/third_party/libvpx/unpack_lib_posix.sh',
        '-d', '<(unpack_lib_output_dir)',
        '-f', '<(unpack_lib_name)',
        '<@(unpack_lib_search_path_list)',
        '<@(ar_cmd)',
      ],
      'process_output_as_sources': 1,
    },
  ],
}
