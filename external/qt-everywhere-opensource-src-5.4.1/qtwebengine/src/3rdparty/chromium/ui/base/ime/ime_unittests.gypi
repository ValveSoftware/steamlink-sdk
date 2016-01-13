# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'sources': [
    'candidate_window_unittest.cc',
    'chromeos/character_composer_unittest.cc',
    'composition_text_util_pango_unittest.cc',
    'input_method_base_unittest.cc',
    'input_method_chromeos_unittest.cc',
    'remote_input_method_win_unittest.cc',
    'win/imm32_manager_unittest.cc',
    'win/tsf_input_scope_unittest.cc',
  ],
  'conditions': [
    ['chromeos==0 or use_x11==0', {
      'sources!': [
        'chromeos/character_composer_unittest.cc',
        'input_method_chromeos_unittest.cc',
      ],
    }],
    ['use_x11==0', {
      'sources!': [
        'composition_text_util_pango_unittest.cc',
      ],
    }],
  ],
}
