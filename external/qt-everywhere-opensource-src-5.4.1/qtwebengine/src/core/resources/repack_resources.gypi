# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'repack_path': '<(chromium_src_dir)/tools/grit/grit/format/repack.py',
    'pak_inputs': [
      '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/webkit/devtools_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources_100_percent.pak',
      '<(SHARED_INTERMEDIATE_DIR)/webkit/blink_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources_100_percent.pak',
      '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/webui_resources.pak',
      '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources_100_percent.pak',
    ],
  },
  'inputs': [
    '<(repack_path)',
    '<@(pak_inputs)',
  ],
  'outputs': [
    '<(SHARED_INTERMEDIATE_DIR)/repack/qtwebengine_resources.pak',
  ],
  'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
}
