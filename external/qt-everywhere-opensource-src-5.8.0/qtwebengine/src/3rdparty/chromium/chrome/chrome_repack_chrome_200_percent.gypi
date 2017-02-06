# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  # GN version: //chrome:repack_chrome_200_percent
  'action_name': 'repack_chrome_resources_200_percent',
  'variables': {
    'pak_inputs': [
      '<(SHARED_INTERMEDIATE_DIR)/components/components_resources_200_percent.pak',
      '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_200_percent.pak',
      '<(grit_out_dir)/renderer_resources_200_percent.pak',
      '<(grit_out_dir)/theme_resources_200_percent.pak',
    ],
    'pak_output': '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_200_percent.pak',
    'conditions': [
      ['OS != "ios"', {
        'pak_inputs': [
          '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_image_resources_200_percent.pak',
          '<(SHARED_INTERMEDIATE_DIR)/content/app/resources/content_resources_200_percent.pak',
        ],
      }],
      ['toolkit_views==1', {
        'pak_inputs': [
          '<(SHARED_INTERMEDIATE_DIR)/ui/views/resources/views_resources_200_percent.pak',
        ],
      }],
      ['use_ash==1', {
        'pak_inputs': [
          '<(SHARED_INTERMEDIATE_DIR)/ash/resources/ash_resources_200_percent.pak',
        ],
      }],
      ['chromeos==1', {
        'pak_inputs': [
          '<(SHARED_INTERMEDIATE_DIR)/ui/chromeos/resources/ui_chromeos_resources_200_percent.pak',
        ],
      }],
      ['enable_extensions==1', {
        'pak_inputs': [
          '<(SHARED_INTERMEDIATE_DIR)/extensions/extensions_browser_resources_200_percent.pak',
        ],
      }],
      ['enable_app_list==1', {
        'pak_inputs': [
          '<(SHARED_INTERMEDIATE_DIR)/ui/app_list/resources/app_list_resources_200_percent.pak',
        ],
      }],
    ],
  },
  'includes': [ '../build/repack_action.gypi' ],
}
