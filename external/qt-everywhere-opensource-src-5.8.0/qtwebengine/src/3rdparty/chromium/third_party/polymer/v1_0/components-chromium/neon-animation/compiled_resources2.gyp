# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# NOTE: Created with generate_compiled_resources_gyp.py, please do not edit.
{
  'targets': [
    {
      'target_name': 'neon-animatable-behavior-extracted',
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'neon-animatable-extracted',
      'dependencies': [
        '../iron-resizable-behavior/compiled_resources2.gyp:iron-resizable-behavior-extracted',
        'neon-animatable-behavior-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'neon-animated-pages-extracted',
      'dependencies': [
        '../iron-resizable-behavior/compiled_resources2.gyp:iron-resizable-behavior-extracted',
        '../iron-selector/compiled_resources2.gyp:iron-selectable-extracted',
        'animations/compiled_resources2.gyp:opaque-animation-extracted',
        'neon-animation-runner-behavior-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'neon-animation-behavior-extracted',
      'dependencies': [
        '../iron-meta/compiled_resources2.gyp:iron-meta-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'neon-animation-extracted',
      'dependencies': [
        'neon-animatable-behavior-extracted',
        'neon-animatable-extracted',
        'neon-animated-pages-extracted',
        'neon-animation-behavior-extracted',
        'neon-animation-runner-behavior-extracted',
        'neon-shared-element-animatable-behavior-extracted',
        'neon-shared-element-animation-behavior-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'neon-animation-runner-behavior-extracted',
      'dependencies': [
        '../iron-meta/compiled_resources2.gyp:iron-meta-extracted',
        'neon-animatable-behavior-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'neon-animations-extracted',
      'dependencies': [
        'animations/compiled_resources2.gyp:cascaded-animation-extracted',
        'animations/compiled_resources2.gyp:fade-in-animation-extracted',
        'animations/compiled_resources2.gyp:fade-out-animation-extracted',
        'animations/compiled_resources2.gyp:hero-animation-extracted',
        'animations/compiled_resources2.gyp:opaque-animation-extracted',
        'animations/compiled_resources2.gyp:reverse-ripple-animation-extracted',
        'animations/compiled_resources2.gyp:ripple-animation-extracted',
        'animations/compiled_resources2.gyp:scale-down-animation-extracted',
        'animations/compiled_resources2.gyp:scale-up-animation-extracted',
        'animations/compiled_resources2.gyp:slide-down-animation-extracted',
        'animations/compiled_resources2.gyp:slide-from-bottom-animation-extracted',
        'animations/compiled_resources2.gyp:slide-from-left-animation-extracted',
        'animations/compiled_resources2.gyp:slide-from-right-animation-extracted',
        'animations/compiled_resources2.gyp:slide-from-top-animation-extracted',
        'animations/compiled_resources2.gyp:slide-left-animation-extracted',
        'animations/compiled_resources2.gyp:slide-right-animation-extracted',
        'animations/compiled_resources2.gyp:slide-up-animation-extracted',
        'animations/compiled_resources2.gyp:transform-animation-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'neon-shared-element-animatable-behavior-extracted',
      'dependencies': [
        'neon-animatable-behavior-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'neon-shared-element-animation-behavior-extracted',
      'dependencies': [
        'neon-animation-behavior-extracted',
      ],
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'web-animations-extracted',
      'includes': ['../../../../closure_compiler/compile_js2.gypi'],
    },
  ],
}
