# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['chromeos==1', {
      'targets': [
        {
          'target_name': 'chromevox_third_party_resources',
          'type': 'none',
          'copies': [
            # TODO(plundblad): Some of these css files are forks of
            # cs from Chrome's web ui.  Consider consolidating those.
            {
              'destination': '<(PRODUCT_DIR)/resources/chromeos/chromevox/chromevox/background',
              'files': [
                'chromevox/background/chrome_shared2.css',
                'chromevox/background/options.css',
                'chromevox/background/options_widgets.css',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/resources/chromeos/chromevox/chromevox/injected',
              'files': [
                'chromevox/injected/mathjax_external_util.js',
                'chromevox/injected/mathjax.js',
              ],
            },
          ],
        },
      ],
    }],
  ],
}
