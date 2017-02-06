# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  # GN version: //chrome:repack_chrome_300_percent
  'action_name': 'repack_chrome_resources_300_percent',
  'variables': {
    'pak_inputs': [
      # Only iOS generates 300 percent pak files so the inputs will need to be
      # updated if other platforms need 300 percent support.
      '<(SHARED_INTERMEDIATE_DIR)/components/components_resources_300_percent.pak',
      '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_300_percent.pak',
      # TODO(ios): Remove the dependency on renderer_resources.
      '<(grit_out_dir)/renderer_resources_300_percent.pak',
      '<(grit_out_dir)/theme_resources_300_percent.pak',
    ],
    'pak_output': '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_300_percent.pak',
  },
  'includes': [ '../build/repack_action.gypi' ],
}
