# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# NOTE: Created with generate_compiled_resources_gyp.py, please do not edit.
{
  'targets': [
    {
      'target_name': 'paper-dropdown-menu-extracted',
      'dependencies': [
        '../iron-a11y-keys-behavior/compiled_resources2.gyp:iron-a11y-keys-behavior-extracted',
        '../iron-behaviors/compiled_resources2.gyp:iron-button-state-extracted',
        '../iron-behaviors/compiled_resources2.gyp:iron-control-state-extracted',
        '../iron-form-element-behavior/compiled_resources2.gyp:iron-form-element-behavior-extracted',
        '../iron-icon/compiled_resources2.gyp:iron-icon-extracted',
        '../iron-validatable-behavior/compiled_resources2.gyp:iron-validatable-behavior-extracted',
        '../paper-input/compiled_resources2.gyp:paper-input-extracted',
        '../paper-menu-button/compiled_resources2.gyp:paper-menu-button-extracted',
        '../paper-ripple/compiled_resources2.gyp:paper-ripple-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'paper-dropdown-menu-icons-extracted',
      'dependencies': [
        '../iron-iconset-svg/compiled_resources2.gyp:iron-iconset-svg-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'paper-dropdown-menu-light-extracted',
      'dependencies': [
        '../iron-a11y-keys-behavior/compiled_resources2.gyp:iron-a11y-keys-behavior-extracted',
        '../iron-behaviors/compiled_resources2.gyp:iron-button-state-extracted',
        '../iron-behaviors/compiled_resources2.gyp:iron-control-state-extracted',
        '../iron-form-element-behavior/compiled_resources2.gyp:iron-form-element-behavior-extracted',
        '../iron-validatable-behavior/compiled_resources2.gyp:iron-validatable-behavior-extracted',
        '../paper-behaviors/compiled_resources2.gyp:paper-ripple-behavior-extracted',
        '../paper-menu-button/compiled_resources2.gyp:paper-menu-button-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'paper-dropdown-menu-shared-styles-extracted',
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
  ],
}
