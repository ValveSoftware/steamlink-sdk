# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
   },
  'target_defaults': {
    'include_dirs': [
      '<(DEPTH)',
    ],
    'libraries': [
      'userenv.lib',
    ],
  },
  'targets' : [
    {
      'target_name': 'virtual_driver_lib<(virtual_driver_suffix)',
      'type': 'static_library',
      'sources': [
        '<(DEPTH)/cloud_print/common/win/cloud_print_utils.cc',
        '<(DEPTH)/cloud_print/common/win/cloud_print_utils.h',
        'virtual_driver_consts.cc',
        'virtual_driver_consts.h',
        'virtual_driver_helpers.cc',
        'virtual_driver_helpers.h',
      ],
    },
    {
      'target_name': 'gcp_portmon_lib<(virtual_driver_suffix)',
      'type': 'static_library',
      'sources': [
        'port_monitor/port_monitor.cc',
        'port_monitor/port_monitor.h',
      ],
      'dependencies': [
        'virtual_driver_lib<(virtual_driver_suffix)',
      ],
    },
    {
      'target_name': 'gcp_portmon<(virtual_driver_suffix)',
      'type': 'loadable_module',
      'sources': [
        'port_monitor/port_monitor.def',
        'port_monitor/port_monitor_dll.cc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/gcp_portmon<(virtual_driver_suffix)_dll_version.rc',
      ],
      'dependencies': [
        'gcp_portmon_lib<(virtual_driver_suffix)',
        '<(DEPTH)/chrome/chrome.gyp:chrome_version_header',
        '<(DEPTH)/cloud_print/cloud_print_resources.gyp:cloud_print_version_resources',
      ],
      'include_dirs': [
        # To allow including "version.h"
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
    },
  ],
}
