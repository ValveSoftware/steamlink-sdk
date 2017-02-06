# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/net_log
      'target_name': 'net_log',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        'data_reduction_proxy_core_common',
        'version_info',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'net_log/chrome_net_log.cc',
        'net_log/chrome_net_log.h',
        'net_log/net_export_ui_constants.cc',
        'net_log/net_export_ui_constants.h',
        'net_log/net_log_temp_file.cc',
        'net_log/net_log_temp_file.h',
      ],
    },
  ],
}
