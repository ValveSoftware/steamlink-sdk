# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'visitedlink_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_common',
        '../ipc/ipc.gyp:ipc',
        '../url/url.gyp:url_lib',
      ],
      'sources': [
        'visitedlink/common/visitedlink_common.cc',
        'visitedlink/common/visitedlink_common.h',
        'visitedlink/common/visitedlink_message_generator.cc',
        'visitedlink/common/visitedlink_message_generator.h',
        'visitedlink/common/visitedlink_messages.h',
      ],
    },
    {
      'target_name': 'visitedlink_browser',
      'type': 'static_library',
      'include_dirs': [
        '../skia/config',
      ],
      'dependencies': [
        'visitedlink_common',
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../content/content.gyp:content_common',
      ],
      'sources': [
        'visitedlink/browser/visitedlink_delegate.h',
        'visitedlink/browser/visitedlink_event_listener.cc',
        'visitedlink/browser/visitedlink_event_listener.h',
        'visitedlink/browser/visitedlink_master.cc',
        'visitedlink/browser/visitedlink_master.h',
      ],
    }
  ],
  'conditions': [
    ['OS != "ios"', {
      'targets': [
        {
          'target_name': 'visitedlink_renderer',
          'type': 'static_library',
          'dependencies': [
            'visitedlink_common',
            '../base/base.gyp:base',
            '../content/content.gyp:content_common',
            '../content/content.gyp:content_renderer',
            '../third_party/WebKit/public/blink.gyp:blink',
          ],
          'sources': [
            'visitedlink/renderer/visitedlink_slave.cc',
            'visitedlink/renderer/visitedlink_slave.h',
          ],
        },
      ],
    }],
  ],
}
