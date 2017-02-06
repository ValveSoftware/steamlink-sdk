# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromevox_assets_images': [
      'images/chromevox-128.png',
      'images/chromevox-16.png',
      'images/chromevox-19.png',
      'images/chromevox-48.png',
      'images/chromevox.svg',
      'images/close-19.png',
      'images/close-hover-19.png',
      'images/options-19.png',
      'images/options-hover-19.png',
      'images/triangle-6.png',
    ],
    'chromevox_assets_chromevox_background_earcons': [
      'chromevox/background/earcons/alert_modal.ogg',
      'chromevox/background/earcons/alert_nonmodal.ogg',
      'chromevox/background/earcons/button.ogg',
      'chromevox/background/earcons/check_off.ogg',
      'chromevox/background/earcons/check_on.ogg',
      'chromevox/background/earcons/editable_text.ogg',
      'chromevox/background/earcons/invalid_keypress.ogg',
      'chromevox/background/earcons/link.ogg',
      'chromevox/background/earcons/list_item.ogg',
      'chromevox/background/earcons/listbox.ogg',
      'chromevox/background/earcons/long_desc.ogg',
      'chromevox/background/earcons/math.ogg',
      'chromevox/background/earcons/object_close.ogg',
      'chromevox/background/earcons/object_enter.ogg',
      'chromevox/background/earcons/object_exit.ogg',
      'chromevox/background/earcons/object_open.ogg',
      'chromevox/background/earcons/object_select.ogg',
      'chromevox/background/earcons/page_finish_loading.ogg',
      'chromevox/background/earcons/page_start_loading.ogg',
      'chromevox/background/earcons/recover_focus.ogg',
      'chromevox/background/earcons/selection.ogg',
      'chromevox/background/earcons/selection_reverse.ogg',
      'chromevox/background/earcons/skip.ogg',
      'chromevox/background/earcons/wrap.ogg',
      'chromevox/background/earcons/wrap_edge.ogg',
    ],
    'chromevox_assets_chromevox_background_keymaps': [
      'chromevox/background/keymaps/classic_keymap.json',
      'chromevox/background/keymaps/experimental.json',
      'chromevox/background/keymaps/flat_keymap.json',
      'chromevox/background/keymaps/next_keymap.json',
    ],
    'chromevox_assets_chromevox_background_mathmaps_functions': [
      'chromevox/background/mathmaps/functions/algebra.json',
      'chromevox/background/mathmaps/functions/elementary.json',
      'chromevox/background/mathmaps/functions/hyperbolic.json',
      'chromevox/background/mathmaps/functions/trigonometry.json',
    ],
    'chromevox_assets_chromevox_background_mathmaps_symbols': [
      'chromevox/background/mathmaps/symbols/greek-capital.json',
      'chromevox/background/mathmaps/symbols/greek-mathfonts.json',
      'chromevox/background/mathmaps/symbols/greek-scripts.json',
      'chromevox/background/mathmaps/symbols/greek-small.json',
      'chromevox/background/mathmaps/symbols/greek-symbols.json',
      'chromevox/background/mathmaps/symbols/hebrew_letters.json',
      'chromevox/background/mathmaps/symbols/latin-lower-double-accent.json',
      'chromevox/background/mathmaps/symbols/latin-lower-normal.json',
      'chromevox/background/mathmaps/symbols/latin-lower-phonetic.json',
      'chromevox/background/mathmaps/symbols/latin-lower-single-accent.json',
      'chromevox/background/mathmaps/symbols/latin-mathfonts.json',
      'chromevox/background/mathmaps/symbols/latin-rest.json',
      'chromevox/background/mathmaps/symbols/latin-upper-double-accent.json',
      'chromevox/background/mathmaps/symbols/latin-upper-normal.json',
      'chromevox/background/mathmaps/symbols/latin-upper-single-accent.json',
      'chromevox/background/mathmaps/symbols/math_angles.json',
      'chromevox/background/mathmaps/symbols/math_arrows.json',
      'chromevox/background/mathmaps/symbols/math_characters.json',
      'chromevox/background/mathmaps/symbols/math_delimiters.json',
      'chromevox/background/mathmaps/symbols/math_digits.json',
      'chromevox/background/mathmaps/symbols/math_geometry.json',
      'chromevox/background/mathmaps/symbols/math_harpoons.json',
      'chromevox/background/mathmaps/symbols/math_non_characters.json',
      'chromevox/background/mathmaps/symbols/math_symbols.json',
      'chromevox/background/mathmaps/symbols/math_whitespace.json',
      'chromevox/background/mathmaps/symbols/other_stars.json',
    ],
    'chromevox_assets_cvox2_background_earcons': [
      'cvox2/background/earcons/control.wav',
      'cvox2/background/earcons/selection_reverse.wav',
      'cvox2/background/earcons/selection.wav',
      'cvox2/background/earcons/skim.wav',
      'cvox2/background/earcons/small_room_2.wav',
      'cvox2/background/earcons/static.wav',
    ],
  },
  'targets': [
    {
      'target_name': 'chromevox_assets',
      'type': 'none',
      'copies': [
        {
          'destination': '<(chromevox_dest_dir)/images',
          'files': [
            '<@(chromevox_assets_images)',
          ],
        },
        {
          'destination': '<(chromevox_dest_dir)/chromevox/background/earcons',
          'files': [
            '<@(chromevox_assets_chromevox_background_earcons)',
          ],
        },
        {
          'destination': '<(chromevox_dest_dir)/chromevox/background/keymaps',
          'files': [
            '<@(chromevox_assets_chromevox_background_keymaps)',
          ],
        },
        {
          'destination': '<(chromevox_dest_dir)/chromevox/background/mathmaps/functions',
          'files': [
            '<@(chromevox_assets_chromevox_background_mathmaps_functions)',
          ],
        },
        {
          'destination': '<(chromevox_dest_dir)/chromevox/background/mathmaps/symbols',
          'files': [
            '<@(chromevox_assets_chromevox_background_mathmaps_symbols)',
          ],
        },
        {
          'destination': '<(chromevox_dest_dir)/cvox2/background/earcons',
          'files': [
            '<@(chromevox_assets_cvox2_background_earcons)',
          ],
        },
      ],
    },
  ],
}
