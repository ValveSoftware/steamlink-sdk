# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'dest_dir': '<(PRODUCT_DIR)/ui/accessibility/extensions',
  },
  'targets': [
    {
      'target_name': 'accessibility_extensions',
      'type': 'none',
      'dependencies': [
        'alt',
        'animation',
        'caretbrowsing',
        'colorenhancer',
        'highcontrast',
        'longdesc',
      ]
    },
    {
      'target_name': 'alt',
      'type': 'none',
      'copies': [
        {
          'destination': '<(dest_dir)/alt',
          'files': [
            'alt/background.js',
            'alt/hide-images.css',
            'alt/hide-images.js',
            'alt/manifest.json',
          ]
        },
        {
          'destination': '<(dest_dir)/alt/images',
          'files': [
            'alt/images/icon-16.png',
            'alt/images/icon-19.png',
            'alt/images/icon-38.png',
            'alt/images/icon-48.png',
            'alt/images/icon-128.png',
            'alt/images/speech-16.png',
            'alt/images/speech-missing-alt-16.png',
            'alt/images/statusbarButtonGlyphs.png',
          ]
        },
        {
          'destination': '<(dest_dir)/alt/lib',
          'files': [
            '../../../third_party/accessibility-audit/axs_testing.js',
          ]
        },
      ],
      'actions': [
        {
          'action_name': 'alt_strings',
          'variables': {
            'grit_grd_file': 'strings/accessibility_extensions_strings.grd',
            'grit_out_dir': '<(dest_dir)/alt',
            # We don't generate any RC files, so no resource_ds file is needed.
            'grit_resource_ids': '',
          },
          'includes': [ '../../../build/grit_action.gypi' ],
        },
      ],
    },
    {
      'target_name': 'animation',
      'type': 'none',
      'copies': [
        {
          'destination': '<(dest_dir)/animation',
          'files': [
            'animation/manifest.json',
            'animation/popup.html',
            'animation/popup.js',
            'animation/animation.png',
          ]
        }
      ],
      'actions': [
        {
          'action_name': 'animation_strings',
          'variables': {
            'grit_grd_file': 'strings/accessibility_extensions_strings.grd',
            'grit_out_dir': '<(dest_dir)/animation',
            # We don't generate any RC files, so no resource_ds file is needed.
            'grit_resource_ids': '',
          },
          'includes': [ '../../../build/grit_action.gypi' ],
        },
      ],
    },
    {
      'target_name': 'caretbrowsing',
      'type': 'none',
      'copies': [
        {
          'destination': '<(dest_dir)/caretbrowsing',
          'files': [
            '../../../third_party/accessibility-audit/axs_testing.js',
            'caretbrowsing/background.js',
            'caretbrowsing/caret_128.png',
            'caretbrowsing/caret_16.png',
            'caretbrowsing/caret_19.png',
            'caretbrowsing/caret_19_on.png',
            'caretbrowsing/caret_48.png',
            'caretbrowsing/caretbrowsing.css',
            'caretbrowsing/caretbrowsing.js',
            'caretbrowsing/increase_brightness.png',
            'caretbrowsing/manifest.json',
            'caretbrowsing/options.html',
            'caretbrowsing/options.js',
            'caretbrowsing/traverse_util.js',
          ]
        }
      ],
      'actions': [
        {
          'action_name': 'caretbrowsing_strings',
          'variables': {
            'grit_grd_file': 'strings/accessibility_extensions_strings.grd',
            'grit_out_dir': '<(dest_dir)/caretbrowsing',
            # We don't generate any RC files, so no resource_ds file is needed.
            'grit_resource_ids': '',
          },
          'includes': [ '../../../build/grit_action.gypi' ],
        },
      ],
    },
    {
      'target_name': 'colorenhancer',
      'type': 'none',
      'copies': [
        {
          'destination': '<(dest_dir)/colorenhancer',
          'files': [
            'colorenhancer/manifest.json',
          ]
        },
        {
          'destination': '<(dest_dir)/colorenhancer/src',
          'files': [
            'colorenhancer/src/background.js',
            'colorenhancer/src/common.js',
            'colorenhancer/src/cvd.js',
            'colorenhancer/src/popup.html',
            'colorenhancer/src/popup.js',
            'colorenhancer/src/storage.js',
          ]
        },
        {
          'destination': '<(dest_dir)/colorenhancer/res',
          'files': [
            'colorenhancer/res/cvd-128.png',
            'colorenhancer/res/cvd-16.png',
            'colorenhancer/res/cvd-19.png',
            'colorenhancer/res/cvd-38.png',
            'colorenhancer/res/cvd-48.png',
            'colorenhancer/res/cvd.css',
            'colorenhancer/res/setup.css',
          ]
        },
      ],
      'actions': [
        {
          'action_name': 'colorenhancer_strings',
          'variables': {
            'grit_grd_file': 'strings/accessibility_extensions_strings.grd',
            'grit_out_dir': '<(dest_dir)/colorenhancer',
            # We don't generate any RC files, so no resource_ds file is needed.
            'grit_resource_ids': '',
          },
          'includes': [ '../../../build/grit_action.gypi' ],
        },
      ],
    },
    {
      'target_name': 'highcontrast',
      'type': 'none',
      'copies': [
        {
          'destination': '<(dest_dir)/highcontrast',
          'files': [
            'highcontrast/background.js',
            'highcontrast/common.js',
            'highcontrast/highcontrast-128.png',
            'highcontrast/highcontrast-16.png',
            'highcontrast/highcontrast-19.png',
            'highcontrast/highcontrast-48.png',
            'highcontrast/highcontrast.css',
            'highcontrast/highcontrast.js',
            'highcontrast/manifest.json',
            'highcontrast/popup.html',
            'highcontrast/popup.js',
          ]
        }
      ],
      'actions': [
        {
          'action_name': 'highcontrast_strings',
          'variables': {
            'grit_grd_file': 'strings/accessibility_extensions_strings.grd',
            'grit_out_dir': '<(dest_dir)/highcontrast',
            # We don't generate any RC files, so no resource_ds file is needed.
            'grit_resource_ids': '',
          },
          'includes': [ '../../../build/grit_action.gypi' ],
        },
      ],
    },
    {
      'target_name': 'longdesc',
      'type': 'none',
      'copies': [
        {
          'destination': '<(dest_dir)/longdesc',
          'files': [
            'longdesc/background.js',
            'longdesc/border.css',
            'longdesc/icon.png',
            'longdesc/icon-128.png',
            'longdesc/icon-48.png',
            'longdesc/icon-16.png',
            'longdesc/lastRightClick.js',
            'longdesc/manifest.json',
            'longdesc/options.html',
            'longdesc/options.js',
          ]
        }
      ],
      'actions': [
        {
          'action_name': 'longdesc_strings',
          'variables': {
            'grit_grd_file': 'strings/accessibility_extensions_strings.grd',
            'grit_out_dir': '<(dest_dir)/longdesc',
            # We don't generate any RC files, so no resource_ds file is needed.
            'grit_resource_ids': '',
          },
          'includes': [ '../../../build/grit_action.gypi' ],
        },
      ],
    },
  ],
}
