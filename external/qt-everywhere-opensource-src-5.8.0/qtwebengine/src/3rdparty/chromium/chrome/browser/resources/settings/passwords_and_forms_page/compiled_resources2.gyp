# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'passwords_and_forms_page',
      'dependencies': [
        '../prefs/compiled_resources2.gyp:prefs_behavior',
        '../settings_page/compiled_resources2.gyp:settings_animated_pages',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(EXTERNS_GYP):passwords_private',
        '<(EXTERNS_GYP):settings_private',
        'autofill_section',
        'passwords_section',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'autofill_section',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/cr_shared_menu/compiled_resources2.gyp:cr_shared_menu',
        '<(EXTERNS_GYP):autofill_private',
        'credit_card_edit_dialog',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'credit_card_edit_dialog',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:i18n_behavior',
        '<(EXTERNS_GYP):autofill_private',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'passwords_section',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/cr_shared_menu/compiled_resources2.gyp:cr_shared_menu',
        '<(EXTERNS_GYP):passwords_private',
        'password_edit_dialog',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'password_edit_dialog',
      'dependencies': [
        '<(EXTERNS_GYP):passwords_private',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
