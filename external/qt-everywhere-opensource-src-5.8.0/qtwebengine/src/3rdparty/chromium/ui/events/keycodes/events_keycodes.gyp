# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/events/keycodes:xkb
      'target_name': 'keycodes_xkb',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/ui/events/events.gyp:dom_keycode_converter',
      ],
      'sources': [
        'keyboard_code_conversion_xkb.cc',
        'keyboard_code_conversion_xkb.h',
        'scoped_xkb.h',
        'xkb_keysym.h',
      ],
    },
  ],
  'conditions': [
    ['use_x11==1', {
      'targets': [
        {
          # GN version: //ui/events/keycodes:x11
          'target_name': 'keycodes_x11',
          'type': '<(component)',
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/build/linux/system.gyp:x11',
            '<(DEPTH)/ui/gfx/x/gfx_x11.gyp:gfx_x11',
            '../events.gyp:dom_keycode_converter',
            'keycodes_xkb',
          ],
          'defines': [
            'KEYCODES_X_IMPLEMENTATION',
          ],
          'sources': [
            'keycodes_x_export.h',
            'keyboard_code_conversion_x.cc',
            'keyboard_code_conversion_x.h',
            'keysym_to_unicode.cc',
            'keysym_to_unicode.h',
          ],
        },
      ],
    }],
  ],
}
