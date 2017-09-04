# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# NOTE: Created with generate_compiled_resources_gyp.py, please do not edit.
{
  'targets': [
    {
      'target_name': 'app-scroll-effects-behavior-extracted',
      'dependencies': [
        '../../iron-scroll-target-behavior/compiled_resources2.gyp:iron-scroll-target-behavior-extracted',
        '../helpers/compiled_resources2.gyp:helpers-extracted',
      ],
      'includes': ['../../../../../closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'app-scroll-effects-extracted',
      'dependencies': [
        'effects/compiled_resources2.gyp:blend-background-extracted',
        'effects/compiled_resources2.gyp:fade-background-extracted',
        'effects/compiled_resources2.gyp:material-extracted',
        'effects/compiled_resources2.gyp:parallax-background-extracted',
        'effects/compiled_resources2.gyp:resize-snapped-title-extracted',
        'effects/compiled_resources2.gyp:resize-title-extracted',
        'effects/compiled_resources2.gyp:waterfall-extracted',
      ],
      'includes': ['../../../../../closure_compiler/compile_js2.gypi'],
    },
  ],
}
