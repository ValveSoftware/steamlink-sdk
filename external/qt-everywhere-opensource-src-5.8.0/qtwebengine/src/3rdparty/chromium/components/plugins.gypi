# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS!="ios"', {
      'targets': [
        {
          # GN version: //components/plugins/renderer
          'target_name': 'plugins_renderer',
          'type': 'static_library',
          'dependencies': [
            '../gin/gin.gyp:gin',
            '../skia/skia.gyp:skia',
            '../third_party/WebKit/public/blink.gyp:blink',
            '../third_party/re2/re2.gyp:re2',
            '../v8/src/v8.gyp:v8',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            # Note: sources list duplicated in GN build.
            'plugins/renderer/plugin_placeholder.cc',
            'plugins/renderer/plugin_placeholder.h',
            'plugins/renderer/webview_plugin.cc',
            'plugins/renderer/webview_plugin.h',
          ],
          'conditions' : [
            ['enable_plugins==1', {
              'sources': [
                # Note: sources list duplicated in GN build.
                'plugins/renderer/loadable_plugin_placeholder.cc',
                'plugins/renderer/loadable_plugin_placeholder.h',
              ]
            }],
            ['OS=="android"', {
              'sources': [
                # Note: sources list duplicated in GN build.
                'plugins/renderer/mobile_youtube_plugin.cc',
                'plugins/renderer/mobile_youtube_plugin.h',
              ]
            }],
          ],
        },
      ],
    }],
  ],
}
