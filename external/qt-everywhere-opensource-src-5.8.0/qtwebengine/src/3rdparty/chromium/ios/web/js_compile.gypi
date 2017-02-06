# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file should be deprecated in favor of js_compile_checked.gypi or
# eventually third_party/closure_compiler/compile_js.gypi as iOS JS code
# becomes error free. See http://crbug.com/487804
{
  'variables': {
    'closure_compiler_path': '<(DEPTH)/third_party/closure_compiler/compiler/compiler.jar',
    'compile_javascript%': 1,
  },
  'conditions': [
    ['compile_javascript==1', {
      'rules': [
        {
          'rule_name': 'jscompilation',
          'extension': 'js',
          'inputs': [
            '<(closure_compiler_path)',
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
            '--js',
            '<(RULE_INPUT_PATH)',
            '--js_output_file',
            '<@(_outputs)',
          ],
          'message': 'Running closure compiler on <(RULE_INPUT_NAME)',
        }  # rule_name: jscompilation
      ]},
     {  # else
      'rules': [
        {
          'rule_name': 'jscompilation',
          'extension': 'js',
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/<(RULE_INPUT_NAME)',
          ],
          'action': [
            'cp',
            '<(RULE_INPUT_PATH)',
            '<@(_outputs)',
          ],
        }
      ]}  # rule_name: jscompilation
    ]  # condition: compile_javascript
  ]  # conditions
}
