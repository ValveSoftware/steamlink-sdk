# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file defines rules that allow you to include JavaScript tests in
# your unittests target.

# To add JS unittests to an existing unittest target, first include
# 'js_unittest_vars.gypi' at the top of your GYP file to define the required
# variables:
#
#  'includes': [
#    '<(DEPTH)/chrome/js_unittest_vars.gypi',
#  ],
#
# Then include this rule file in each of your unittest targets:
#
#    {
#      'target_name': 'my_unittests',
#      ...
#      'includes': [
#        '<(DEPTH)/chrome/js_unittest_rules.gypi',
#      ],
#    }
#
# Note that when you run your TestSuite, you'll need to call
# chrome::RegisterPathProvider(). These path providers are required by
# src/chrome/test/base/v8_unit_test.cc to setup and run the tests.

{
    'dependencies': [
      # Used by rule js2unit below.
      '../v8/samples/samples.gyp:v8_shell#host',
    ],
  'rules': [
    {
      'rule_name': 'copyjs',
      'extension': 'js',
      'msvs_external_rule': 1,
      'inputs': [
        '<(DEPTH)/build/cp.py',
      ],
      'outputs': [
        '<(PRODUCT_DIR)/test_data/chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).<(_extension)',
      ],
      'action': [
        'python',
        '<@(_inputs)',
        '<(RULE_INPUT_PATH)',
        '<@(_outputs)',
      ],
    },
    {
      'rule_name': 'js2unit',
      'extension': 'gtestjs',
      'msvs_external_rule': 1,
      'variables': {
        'conditions': [
          ['v8_use_external_startup_data==1', {
            'external_v8': 'y',
          }, {
            'external_v8': 'n',
          }],
        ],
      },
      'inputs': [
        '<(gypv8sh)',
        '<(PRODUCT_DIR)/v8_shell<(EXECUTABLE_SUFFIX)',
        '<(mock_js)',
        '<(test_api_js)',
        '<(js2gtest)',
      ],
      'outputs': [
        '<(INTERMEDIATE_DIR)/chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT)-gen.cc',
        '<(PRODUCT_DIR)/test_data/chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).<(_extension)',
      ],
      'process_outputs_as_sources': 1,
      'action': [
        'python',
        '<@(_inputs)',
        '--external', '<(external_v8)',
        'unit',
        '<(RULE_INPUT_PATH)',
        'chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).<(_extension)',
        '<@(_outputs)',
      ],
    },
  ],
}
