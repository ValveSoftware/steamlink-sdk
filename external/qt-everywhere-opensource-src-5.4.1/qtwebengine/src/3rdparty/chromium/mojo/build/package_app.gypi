# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'type': 'none',
  'dependencies': [
    '<(app_name)',
  ],
  'conditions': [
    ['OS=="android"', {
      'variables': {
        'mojo_app_dir': '<(PRODUCT_DIR)/apps',
        'source_binary': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)<(app_name)<(SHARED_LIB_SUFFIX)',
        'target_binary': '<(mojo_app_dir)/<(SHARED_LIB_PREFIX)<(app_name)<(SHARED_LIB_SUFFIX)',
      },
      'actions': [{
        'action_name': 'strip',
        'inputs': [ '<(source_binary)', ],
        'outputs': [ '<(target_binary)', ],
        'action': [
          '<(android_strip)',
          '<@(_inputs)',
          '--strip-unneeded',
          '-o',
          '<@(_outputs)',
        ],
      }],
    }],
  ],
}
