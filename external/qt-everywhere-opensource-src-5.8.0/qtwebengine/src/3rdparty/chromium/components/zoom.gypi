# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/zoom
      'target_name': 'zoom',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../content/content.gyp:content_common',
        '../ipc/ipc.gyp:ipc',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
      ],
      'sources': [
        'zoom/page_zoom.cc',
        'zoom/page_zoom.h',
        'zoom/page_zoom_constants.cc',
        'zoom/page_zoom_constants.h',
        'zoom/zoom_controller.cc',
        'zoom/zoom_controller.h',
        'zoom/zoom_event_manager.cc',
        'zoom/zoom_event_manager.h',
        'zoom/zoom_event_manager_observer.h',
        'zoom/zoom_observer.h'
      ],
    },
    {
      'target_name': 'zoom_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../content/content.gyp:content_common',
        '../components/components.gyp:zoom',
      ],
      'sources': [
        'zoom/test/zoom_test_utils.cc',
        'zoom/test/zoom_test_utils.h'
      ],
    }
  ],
}
