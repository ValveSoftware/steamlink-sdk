# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/network_session_configurator
      'target_name': 'network_session_configurator',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        'data_reduction_proxy_core_common',
        'network_session_configurator_switches',
        'variations',
        'version_info',
      ],
      'sources': [
        'network_session_configurator/network_session_configurator.cc',
        'network_session_configurator/network_session_configurator.h',
      ],
    },
    {
      # GN version: //components/network_session_configurator_switches
      'target_name': 'network_session_configurator_switches',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'network_session_configurator/switches.cc',
        'network_session_configurator/switches.h',
      ],
    },
  ],
}
