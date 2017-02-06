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
    'chromevox_tests_sources': [
      '../../../../browser/extensions/browsertest_util.cc',
      '../../../../browser/extensions/browsertest_util.h',
      '../../../../browser/ui/webui/web_ui_test_handler.cc',
      '../../../../browser/ui/webui/web_ui_test_handler.h',
      '../../../../test/base/browser_tests_main.cc',
      '../../../../test/base/extension_js_browser_test.cc',
      '../../../../test/base/extension_js_browser_test.h',
      '../../../../test/base/extension_load_waiter_one_shot.cc',
      '../../../../test/base/extension_load_waiter_one_shot.h',
      '../../../../test/base/javascript_browser_test.cc',
      '../../../../test/base/javascript_browser_test.h',
      '../../../../test/base/test_chrome_web_ui_controller_factory.cc',
      '../../../../test/base/test_chrome_web_ui_controller_factory.h',
      '../../../../test/base/web_ui_browser_test.cc',
      '../../../../test/base/web_ui_browser_test.h',
    ],
    'chromevox_tests_unit_gen_include_sources': [
      'testing/assert_additions.js',
      'testing/callback_helper.js',
      'testing/chromevox_unittest_base.js',
      'testing/mock_feedback.js',
    ],
    'chromevox_tests_unitjs_sources': [
      'braille/braille_display_manager_test.unitjs',
      'braille/braille_input_handler_test.unitjs',
      'braille/expanding_braille_translator_test.unitjs',
      'braille/pan_strategy_test.unitjs',
      'common/aria_util_test.unitjs',
      'common/braille_text_handler_test.unitjs',
      'common/braille_util_test.unitjs',
      'common/command_store_test.unitjs',
      'common/content_editable_extractor_test.unitjs',
      'common/cursor_selection_test.unitjs',
      'common/dom_util_test.unitjs',
      'common/editable_text_area_shadow_test.unitjs',
      'common/editable_text_test.unitjs',
      'common/find_util_test.unitjs',
      'common/key_sequence_test.unitjs',
      'common/math_semantic_tree_test.unitjs',
      'common/page_selection_test.unitjs',
      'common/selection_util_test.unitjs',
      'common/spannable_test.unitjs',
      'common/string_util_test.unitjs',
      'chromevox/injected/event_watcher_test.unitjs',
      'chromevox/injected/live_regions_test.unitjs',
      'chromevox/injected/user_commands_test.unitjs',
      'chromevox/injected/navigation_manager_test.unitjs',
      'host/chrome/braille_integration_test.unitjs',
      'testing/mock_feedback_test.unitjs',
      'walkers/character_walker_test.unitjs',
      'walkers/group_walker_test.unitjs',
      'walkers/object_walker_test.unitjs',
      'walkers/layout_line_walker_test.unitjs',
      'walkers/math_shifter_test.unitjs',
      'walkers/sentence_walker_test.unitjs',
      'walkers/structural_line_walker_test.unitjs',
      'walkers/table_walker_test.unitjs',
      'walkers/word_walker_test.unitjs',
    ],
    'chromevox_tests_ext_gen_include_sources': [
      'testing/assert_additions.js',
      'testing/callback_helper.js',
      'testing/chromevox_e2e_test_base.js',
      'testing/chromevox_next_e2e_test_base.js',
      'testing/mock_feedback.js',
    ],
    'chromevox_tests_extjs_sources': [
      'braille/braille_table_test.extjs',
      'braille/braille_translator_manager_test.extjs',
      'braille/liblouis_test.extjs',
      'cvox2/background/automation_util_test.extjs',
      'cvox2/background/background_test.extjs',
      'cvox2/background/cursors_test.extjs',
      'cvox2/background/editing_test.extjs',
      'cvox2/background/i_search_test.extjs',
      'cvox2/background/live_regions_test.extjs',
      'cvox2/background/output_test.extjs',
      'cvox2/background/tree_walker_test.extjs',
      'host/chrome/tts_background_test.extjs',
    ],
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
        '<(DEPTH)/chrome/chrome.gyp:test_support_ui',
        '<(DEPTH)/chrome/chrome_resources.gyp:chrome_resources',
        '<(DEPTH)/chrome/chrome_resources.gyp:chrome_strings',
        '<(DEPTH)/chrome/chrome_resources.gyp:packed_extra_resources',
        '<(DEPTH)/chrome/chrome_resources.gyp:packed_resources',
        '<(DEPTH)/content/content_shell_and_tests.gyp:content_browser_test_base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/v8/src/v8.gyp:v8',
        'chromevox_test_deps_js',
      ],
      'conditions': [
        ['disable_nacl==0 and disable_nacl_untrusted==0', {
          'dependencies': [
            '<(DEPTH)/components/nacl.gyp:nacl_helper',
            '<(DEPTH)/components/nacl_nonsfi.gyp:nacl_helper_nonsfi',
            '<(DEPTH)/native_client/src/trusted/service_runtime/linux/nacl_bootstrap.gyp:nacl_helper_bootstrap',
          ],
        }],
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
          # browser test.  This is meant for unit tests that test individual
          # components without depending on the ChromeVox extension.
          'rule_name': 'js2webui',
          'extension': 'unitjs',
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
            '<(chromevox_test_deps_js_file)',
            '<@(chromevox_tests_unit_gen_include_sources)',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT)-gen.cc',
            '<(PRODUCT_DIR)/test_data/chrome/browser/resources/chromeos/chromevox/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).js',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'python',
            '<(gypv8sh)',
            '<(PRODUCT_DIR)/v8_shell<(EXECUTABLE_SUFFIX)',
            '--deps_js', '<(chromevox_test_deps_js_file)',
            '--external', '<(external_v8)',
            '<(mock_js)',
            '<(test_api_js)',
            '<(js2gtest)',
            'webui',
            '<(RULE_INPUT_PATH)',
            'chrome/browser/resources/chromeos/chromevox/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).unitjs',
            '<@(_outputs)',
          ],
        },
        {
          # A JavaScript test that runs in the ChromeVox background page's
          # isolate.
          'rule_name': 'js2extension',
          'extension': 'extjs',
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
            '<@(chromevox_tests_ext_gen_include_sources)',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT)-gen.cc',
            '<(PRODUCT_DIR)/test_data/chrome/browser/resources/chromeos/chromevox/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).extjs',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'python',
            '<(gypv8sh)',
            '<(PRODUCT_DIR)/v8_shell<(EXECUTABLE_SUFFIX)',
            '--external', '<(external_v8)',
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
        '<@(chromevox_tests_sources)',
        '<@(chromevox_tests_unitjs_sources)',
        '<@(chromevox_tests_extjs_sources)',
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
