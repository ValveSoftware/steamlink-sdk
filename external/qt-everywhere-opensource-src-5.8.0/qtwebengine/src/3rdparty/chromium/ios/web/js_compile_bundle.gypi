# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to build JavaScript bundles using closure compiler.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my_js_target',
#   'type': 'none',
#   'variables': {
#     'closure_entry_point': 'name of the closure module',
#     'js_bundle_files': ['path/to/dependency/file',],
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
#
# Required variables:
#  closure_entry_point - name of the entry point closure module.
#  js_bundle_files - list of js files to build a bundle.

{
  'variables': {
    'closure_compiler_path': '<(DEPTH)/third_party/closure_compiler/compiler/compiler.jar',
  },
  'rules': [
    {
      'rule_name': 'jsbundlecompilation',
      'extension': 'js',
      'inputs': [
        '<(closure_compiler_path)',
        '<@(js_bundle_files)',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(RULE_INPUT_NAME)',
      ],
      'action': [
        'java',
        '-jar',
        '<(closure_compiler_path)',
        '--compilation_level',
        'SIMPLE_OPTIMIZATIONS',
        # Pass every js file to closure compiler. --only_closure_dependencies
        # flag ensures that unnecessary files will not be compiled into the
        # final output file.
        '--js',
        '<@(js_bundle_files)',
        '--js_output_file',
        '<@(_outputs)',
        '--only_closure_dependencies',
        '--closure_entry_point=<(closure_entry_point)',
      ],
      'message': 'Building <(RULE_INPUT_NAME) JavaScript bundle',
    }  # rule_name: jsbundlecompilation
  ]
}
