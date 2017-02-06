# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/metrics_services_manager
      'target_name': 'metrics_services_manager',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'metrics',
        'rappor',
        'variations',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'metrics_services_manager/metrics_services_manager.cc',
        'metrics_services_manager/metrics_services_manager.h',
        'metrics_services_manager/metrics_services_manager_client.h',
      ],
    },
  ],
}
