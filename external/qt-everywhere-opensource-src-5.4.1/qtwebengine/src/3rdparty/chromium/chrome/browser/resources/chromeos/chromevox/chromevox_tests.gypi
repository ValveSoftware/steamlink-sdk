# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../js_unittest_vars.gypi',
  ],
  'variables': {
    'chromevox_test_deps_js_file': '<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/chromevox/test_deps.js',
    'chromevox_test_messages_js_file': '<(PRODUCT_DIR)/test_data/chrome/browser/resources/chromeos/chromevox/host/testing/test_messages.js',
  },
  'targets': [
    {
      'target_name': 'chromevox_tests',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/chrome/chrome.gyp:browser',
        '<(DEPTH)/chrome/chrome.gyp:renderer',
        '<(DEPTH)/chrome/chrome.gyp:test_support_common',
        '<(DEPTH)/chrome/chrome_resources.gyp:chrome_resources',
        '<(DEPTH)/chrome/chrome_resources.gyp:chrome_strings',
        '<(DEPTH)/chrome/chrome_resources.gyp:packed_extra_resources',
        '<(DEPTH)/chrome/chrome_resources.gyp:packed_resources',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
        'chromevox_test_deps_js',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'include_dirs': [
        '<(DEPTH)',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'rules': [
        {
          # A JavaScript test that runs in an environment similar to a webui
          # browser test.
          'rule_name': 'js2webui',
          'extension': 'js',
          'msvs_external_rule': 1,
          'inputs': [
            '<(gypv8sh)',
            '<(PRODUCT_DIR)/d8<(EXECUTABLE_SUFFIX)',
            '<(mock_js)',
            '<(test_api_js)',
            '<(js2gtest)',
            '<(chromevox_test_deps_js_file)',
            'testing/chromevox_unittest_base.js',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT)-gen.cc',
            '<(PRODUCT_DIR)/test_data/chrome/browser/resources/chromeos/chromevox/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).js',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'python',
            '<(gypv8sh)',
            '<(PRODUCT_DIR)/d8<(EXECUTABLE_SUFFIX)',
            '--deps_js', '<(chromevox_test_deps_js_file)',
            '<(mock_js)',
            '<(test_api_js)',
            '<(js2gtest)',
            'webui',
            '<(RULE_INPUT_PATH)',
            'chrome/browser/resources/chromeos/chromevox/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).js',
            '<@(_outputs)',
          ],
        },
        {
          # A JavaScript test that runs in the ChromeVox background page's
          # isolate.
          'rule_name': 'js2extension',
          'extension': 'extjs',
          'msvs_external_rule': 1,
          'inputs': [
            '<(gypv8sh)',
            '<(PRODUCT_DIR)/d8<(EXECUTABLE_SUFFIX)',
            '<(mock_js)',
            '<(test_api_js)',
            '<(js2gtest)',
            'testing/chromevox_unittest_base.js',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT)-gen.cc',
            '<(PRODUCT_DIR)/test_data/chrome/browser/resources/chromeos/chromevox/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).extjs',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'python',
            '<(gypv8sh)',
            '<(PRODUCT_DIR)/d8<(EXECUTABLE_SUFFIX)',
            '<(mock_js)',
            '<(test_api_js)',
            '<(js2gtest)',
            'extension',
            '<(RULE_INPUT_PATH)',
            'chrome/browser/resources/chromeos/chromevox/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).extjs',
            '<@(_outputs)',
          ],
        },
      ],
      'sources': [
        '<(DEPTH)/chrome/browser/ui/webui/web_ui_test_handler.cc',
        '<(DEPTH)/chrome/browser/ui/webui/web_ui_test_handler.h',
        '<(DEPTH)/chrome/test/base/browser_tests_main.cc',
        '<(DEPTH)/chrome/test/base/extension_load_waiter_one_shot.cc',
        '<(DEPTH)/chrome/test/base/extension_load_waiter_one_shot.h',
        '<(DEPTH)/chrome/test/base/extension_js_browser_test.cc',
        '<(DEPTH)/chrome/test/base/extension_js_browser_test.h',
        '<(DEPTH)/chrome/test/base/javascript_browser_test.cc',
        '<(DEPTH)/chrome/test/base/javascript_browser_test.h',
        '<(DEPTH)/chrome/test/base/test_chrome_web_ui_controller_factory.cc',
        '<(DEPTH)/chrome/test/base/test_chrome_web_ui_controller_factory.h',
        '<(DEPTH)/chrome/test/base/web_ui_browser_test.cc',
        '<(DEPTH)/chrome/test/base/web_ui_browser_test.h',
        '<(DEPTH)/chrome/browser/extensions/browsertest_util.cc',
        '<(DEPTH)/chrome/browser/extensions/browsertest_util.h',

        'common/aria_util_test.js',
        'common/braille_util_test.js',
        'common/command_store_test.js',
        'common/cursor_selection_test.js',
        'common/editable_text_area_shadow_test.js',
        'common/editable_text_test.js',
        'common/find_util_test.js',
        'common/key_sequence_test.js',
        'common/math_semantic_tree_test.js',
        'common/page_selection_test.js',
        'common/selection_util_test.js',
        'common/spannable_test.js',
      ],
      'conditions': [
        ['use_chromevox_next==1', {
          'sources': ['../chromevox2/cvox2/background/background.extjs'],
        }],
      ],
    },  # target chromevox_tests
    {
      'target_name': 'chromevox_test_messages_js',
      'type': 'none',
      'actions': [
        {
          'action_name': 'test_messages_js',
          'message': 'Generate <(_target_name)',
          'variables': {
            'english_messages_file': '<(chromevox_dest_dir)/_locales/en/messages.json',
          },
          'inputs': [
            'tools/generate_test_messages.py',
            '<(english_messages_file)',
          ],
          'outputs': [
            '<(chromevox_test_messages_js_file)',
          ],
          'action': [
            'python',
            'tools/generate_test_messages.py',
            '-o', '<(chromevox_test_messages_js_file)',
            '<(english_messages_file)',
          ],
        },
      ],
    },  # target chromevox_test_messages_js
    {
      'target_name': 'chromevox_test_deps_js',
      'type': 'none',
      'actions': [
        {
          'action_name': 'deps_js',
          'message': 'Generate <(_target_name)',
          'variables': {
            # Closure library directory relative to source tree root.
            'closure_dir': 'chrome/third_party/chromevox/third_party/closure-library/closure/goog',
            'depswriter_path': 'tools/generate_deps.py',
            'js_files': [
              '<!@(python tools/find_js_files.py . <(DEPTH)/<(closure_dir))',
              '<(chromevox_test_messages_js_file)',
            ],
          },
          'inputs': [
            '<@(js_files)',
            '<(depswriter_path)',
          ],
          'outputs': [
            '<(chromevox_test_deps_js_file)',
          ],
          'action': [
            'python',
            '<(depswriter_path)',
            '-w', '<(DEPTH)/<(closure_dir):<(closure_dir)',
            '-w', '<(PRODUCT_DIR)/test_data:',
            '-w', ':chrome/browser/resources/chromeos/chromevox',
            '-o', '<(chromevox_test_deps_js_file)',
            '<@(js_files)',
          ],
        },
      ],
      'dependencies': [
        'chromevox_test_messages_js',
      ],
    },  # target chromevox_test_deps_js
  ],
}
