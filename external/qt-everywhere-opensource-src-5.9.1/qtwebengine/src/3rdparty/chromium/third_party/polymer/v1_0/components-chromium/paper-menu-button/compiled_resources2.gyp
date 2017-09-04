# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# NOTE: Created with generate_compiled_resources_gyp.py, please do not edit.
{
  'targets': [
    {
      'target_name': 'paper-menu-button-animations-extracted',
      'dependencies': [
        '../neon-animation/compiled_resources2.gyp:neon-animation-behavior-extracted',
        '<(EXTERNS_GYP):web_animations',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'paper-menu-button-extracted',
      'dependencies': [
        '../iron-a11y-keys-behavior/compiled_resources2.gyp:iron-a11y-keys-behavior-extracted',
        '../iron-behaviors/compiled_resources2.gyp:iron-control-state-extracted',
        '../iron-dropdown/compiled_resources2.gyp:iron-dropdown-extracted',
        '../neon-animation/animations/compiled_resources2.gyp:fade-in-animation-extracted',
        '../neon-animation/animations/compiled_resources2.gyp:fade-out-animation-extracted',
        'paper-menu-button-animations-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
  ],
}
