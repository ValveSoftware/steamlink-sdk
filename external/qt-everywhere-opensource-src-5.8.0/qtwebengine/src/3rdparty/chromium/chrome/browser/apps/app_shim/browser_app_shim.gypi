# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # This is the part of the Chrome browser process responsible for launching
      # and communicating with app_shim processes on Mac.
      #
      # GN version: //chrome/browser/apps/app_shim
      'target_name': 'browser_app_shim',
      'type': 'static_library',
      'dependencies': [
        # Since browser_app_shim and chrome.gyp:browser depend on each other,
        # we omit the dependency on browser here.
        '../content/content.gyp:content_browser',
        '../content/content.gyp:content_common',
      ],
      'sources': [
        'app_shim_handler_mac.cc',
        'app_shim_handler_mac.h',
        'app_shim_host_mac.cc',
        'app_shim_host_mac.h',
        'app_shim_host_manager_mac.h',
        'app_shim_host_manager_mac.mm',
        'extension_app_shim_handler_mac.cc',
        'extension_app_shim_handler_mac.h',
        'unix_domain_socket_acceptor.cc',
        'unix_domain_socket_acceptor.h',
      ],
    },
  ],  # targets
}
