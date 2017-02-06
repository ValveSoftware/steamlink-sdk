# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/browsing_data_ui
      'target_name': 'browsing_data_ui',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        'history_core_browser',
        'sync_driver',
        'version_info',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'browsing_data_ui/history_notice_utils.cc',
        'browsing_data_ui/history_notice_utils.h',
      ],
    },
  ],
}
