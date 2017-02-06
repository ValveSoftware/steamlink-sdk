# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  # GN version: //chrome:repack_chrome_material_200_percent
  'action_name': 'repack_chrome_resources_material_200_percent',
  'variables': {
    'pak_inputs': [
      '<(grit_out_dir)/theme_resources_material_200_percent.pak',
    ],
    'pak_output': '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_material_200_percent.pak',
  },
  'includes': [ '../build/repack_action.gypi' ],
}
