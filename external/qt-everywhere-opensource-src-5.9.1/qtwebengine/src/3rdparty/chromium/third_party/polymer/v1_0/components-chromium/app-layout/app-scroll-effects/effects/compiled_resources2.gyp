# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# NOTE: Created with generate_compiled_resources_gyp.py, please do not edit.
{
  'targets': [
    {
      'target_name': 'blend-background-extracted',
      'dependencies': [
        '../compiled_resources2.gyp:app-scroll-effects-behavior-extracted',
      ],
      'includes': ['../../../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'fade-background-extracted',
      'dependencies': [
        '../compiled_resources2.gyp:app-scroll-effects-behavior-extracted',
      ],
      'includes': ['../../../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'material-extracted',
      'dependencies': [
        '../compiled_resources2.gyp:app-scroll-effects-behavior-extracted',
        'blend-background-extracted',
        'parallax-background-extracted',
        'resize-title-extracted',
        'waterfall-extracted',
      ],
      'includes': ['../../../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'parallax-background-extracted',
      'dependencies': [
        '../compiled_resources2.gyp:app-scroll-effects-behavior-extracted',
      ],
      'includes': ['../../../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'resize-snapped-title-extracted',
      'dependencies': [
        '../compiled_resources2.gyp:app-scroll-effects-behavior-extracted',
      ],
      'includes': ['../../../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'resize-title-extracted',
      'dependencies': [
        '../compiled_resources2.gyp:app-scroll-effects-behavior-extracted',
      ],
      'includes': ['../../../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'waterfall-extracted',
      'dependencies': [
        '../compiled_resources2.gyp:app-scroll-effects-behavior-extracted',
      ],
      'includes': ['../../../../../../closure_compiler/compile_js2.gypi'],
    },
  ],
}
