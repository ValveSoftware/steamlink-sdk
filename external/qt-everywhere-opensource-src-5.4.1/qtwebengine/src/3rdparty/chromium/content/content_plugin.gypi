# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['enable_plugins==1 and OS!="linux"', {
      'dependencies': [
        '../mojo/mojo.gyp:mojo_service_provider_bindings',
        '../skia/skia.gyp:skia',
        '../third_party/WebKit/public/blink.gyp:blink',
        '../third_party/npapi/npapi.gyp:npapi',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
      ],
      'sources': [
        # All .cc, .h, .m, and .mm files under plugins except for tests and
        # mocks.
        'plugin/plugin_channel.cc',
        'plugin/plugin_channel.h',
        'plugin/plugin_interpose_util_mac.mm',
        'plugin/plugin_interpose_util_mac.h',
        'plugin/plugin_main.cc',
        'plugin/plugin_main_mac.mm',
        'plugin/plugin_thread.cc',
        'plugin/plugin_thread.h',
        'plugin/webplugin_accelerated_surface_proxy_mac.cc',
        'plugin/webplugin_accelerated_surface_proxy_mac.h',
        'plugin/webplugin_delegate_stub.cc',
        'plugin/webplugin_delegate_stub.h',
        'plugin/webplugin_proxy.cc',
        'plugin/webplugin_proxy.h',
        'public/plugin/content_plugin_client.h',
      ],
      # These are layered in conditionals in the event other platforms
      # end up using this module as well.
      'conditions': [
        ['OS=="win"', {
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
        }],
      ],
    }],
  ],
}
