# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'action_name': 'repack_resources',
  'variables': {
    'pak_inputs': [
      '<(SHARED_INTERMEDIATE_DIR)/chrome/chrome_unscaled_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/components/components_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/ui/resources/webui_resources.pak',
      '<(grit_out_dir)/browser_resources.pak',
      '<(grit_out_dir)/common_resources.pak',
      '<(grit_out_dir)/invalidations_resources.pak',
      '<(grit_out_dir)/net_internals_resources.pak',
      '<(grit_out_dir)/password_manager_internals_resources.pak',
      '<(grit_out_dir)/policy_resources.pak',
      '<(grit_out_dir)/settings_strings.pak',
      '<(grit_out_dir)/translate_internals_resources.pak',
    ],
    'pak_output': '<(SHARED_INTERMEDIATE_DIR)/repack/resources.pak',
    'conditions': [
      ['branding=="Chrome"', {
        'pak_inputs': [
          '<(grit_out_dir)/settings_google_chrome_strings.pak',
        ],
      }, {
        'pak_inputs': [
          '<(grit_out_dir)/settings_chromium_strings.pak',
        ],
      }],
      ['chromeos==1', {
        'pak_inputs': [
          '<(SHARED_INTERMEDIATE_DIR)/ui/file_manager/file_manager_resources.pak',
          '<(SHARED_INTERMEDIATE_DIR)/components/chrome_apps/chrome_apps_resources.pak',
        ],
      }],
      ['OS != "ios"', {
        'pak_inputs': [
          '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_resources.pak',
          '<(SHARED_INTERMEDIATE_DIR)/content/browser/tracing/tracing_resources.pak',
          '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak',
        ],
      }],
      ['OS != "ios" and OS != "android"', {
        # New paks should be added here by default.
        'pak_inputs': [
          '<(SHARED_INTERMEDIATE_DIR)/blink/devtools_resources.pak',
          '<(grit_out_dir)/component_extension_resources.pak',
          '<(grit_out_dir)/options_resources.pak',
          '<(grit_out_dir)/quota_internals_resources.pak',
          '<(grit_out_dir)/settings_resources.pak',
          '<(grit_out_dir)/sync_file_system_internals_resources.pak',
        ],
      }],
      ['enable_extensions==1', {
        'pak_inputs': [
          '<(SHARED_INTERMEDIATE_DIR)/extensions/extensions_renderer_resources.pak',
          '<(SHARED_INTERMEDIATE_DIR)/extensions/extensions_resources.pak',
          '<(grit_out_dir)/extensions_api_resources.pak',
        ],
      }],
    ],
  },
  'includes': [ '../build/repack_action.gypi' ],
}
