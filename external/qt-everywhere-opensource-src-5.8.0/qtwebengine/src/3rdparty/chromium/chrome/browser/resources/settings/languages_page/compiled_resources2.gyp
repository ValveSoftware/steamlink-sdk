# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'languages',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:promise_resolver',
        '<(EXTERNS_GYP):chrome_send',
        '<(EXTERNS_GYP):input_method_private',
        '<(EXTERNS_GYP):language_settings_private',
        '<(INTERFACES_GYP):input_method_private_interface',
        '<(INTERFACES_GYP):language_settings_private_interface',
        '../prefs/compiled_resources2.gyp:prefs_types',
        '../prefs/compiled_resources2.gyp:prefs',
        'languages_types',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'language_detail_page',
      'dependencies': [
        '../compiled_resources2.gyp:lifetime_browser_proxy',
        '<(DEPTH)/ui/webui/resources/cr_elements/policy/compiled_resources2.gyp:cr_policy_indicator_behavior',
        '<(DEPTH)/ui/webui/resources/js/chromeos/compiled_resources2.gyp:ui_account_tweaks',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
        '<(EXTERNS_GYP):chrome_send',
        'languages',
        'languages_types',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'languages_page',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '../settings_page/compiled_resources2.gyp:settings_animated_pages',
        'languages',
        'languages_types',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'languages_types',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(EXTERNS_GYP):language_settings_private',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'manage_input_methods_page',
      'dependencies': [
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/paper-checkbox/compiled_resources2.gyp:paper-checkbox-extracted',
        '<(EXTERNS_GYP):language_settings_private',
        '../prefs/compiled_resources2.gyp:prefs',
        'languages',
        'languages_types',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'manage_languages_page',
      'dependencies': [
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/paper-checkbox/compiled_resources2.gyp:paper-checkbox-extracted',
        'languages',
        'languages_types',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
