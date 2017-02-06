# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file should be identical to js_compile.gypi except it passes jscomp_error
# flags to the compiler. One should prefer this over js_compile.gypi once the
# JS code being compiled are error free.
#
# This file should be eventually deprecated in favor of
# third_party/closure_compiler/compile_js.gypi once they have the same set of
# jscomp_error flags enabled. See http://crbug.com/487804
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
            '--jscomp_error=accessControls',
            '--jscomp_error=ambiguousFunctionDecl',
            # '--jscomp_error=checkTypes',
            # '--jscomp_error=checkVars',
            '--jscomp_error=constantProperty',
            '--jscomp_error=deprecated',
            '--jscomp_error=externsValidation',
            '--jscomp_error=globalThis',
            '--jscomp_error=invalidCasts',
            # '--jscomp_error=missingProperties',
            # '--jscomp_error=missingReturn',
            '--jscomp_error=nonStandardJsDocs',
            '--jscomp_error=suspiciousCode',
            '--jscomp_error=undefinedNames',
            # '--jscomp_error=undefinedVars',
            '--jscomp_error=unknownDefines',
            '--jscomp_error=uselessCode',
            '--jscomp_error=visibility',
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
