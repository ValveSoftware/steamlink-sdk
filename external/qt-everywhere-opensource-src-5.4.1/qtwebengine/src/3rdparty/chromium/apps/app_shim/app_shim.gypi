# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'app_shim',
      'type': 'static_library',
      # Since app_shim and browser depend on each other, we omit the dependency
      # on browser here.
      'dependencies': [
        '../chrome/chrome_resources.gyp:chrome_strings',
        '../skia/skia.gyp:skia',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(grit_out_dir)',
      ],
      'sources': [
        'app_shim_handler_mac.cc',
        'app_shim_handler_mac.h',
        'app_shim_host_mac.cc',
        'app_shim_host_mac.h',
        'app_shim_host_manager_mac.h',
        'app_shim_host_manager_mac.mm',
        'chrome_main_app_mode_mac.mm',
        'extension_app_shim_handler_mac.cc',
        'extension_app_shim_handler_mac.h',
      ],
    },
  ],  # targets
}
