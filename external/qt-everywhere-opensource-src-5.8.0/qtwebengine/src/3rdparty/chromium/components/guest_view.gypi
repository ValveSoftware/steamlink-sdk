# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    ['OS != "ios"', {
      'targets': [
        {
          'target_name': 'guest_view_browser',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content.gyp:content_browser',
            '../content/content.gyp:content_common',
            'guest_view_common'
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'guest_view/browser/guest_view.h',
            'guest_view/browser/guest_view_base.cc',
            'guest_view/browser/guest_view_base.h',
            'guest_view/browser/guest_view_event.cc',
            'guest_view/browser/guest_view_event.h',
            'guest_view/browser/guest_view_manager.cc',
            'guest_view/browser/guest_view_manager.h',
            'guest_view/browser/guest_view_manager_delegate.cc',
            'guest_view/browser/guest_view_manager_delegate.h',
            'guest_view/browser/guest_view_manager_factory.h',
            'guest_view/browser/guest_view_message_filter.cc',
            'guest_view/browser/guest_view_message_filter.h',
          ],
        },
        {
          'target_name': 'guest_view_common',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../ipc/ipc.gyp:ipc',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'guest_view/common/guest_view_constants.cc',
            'guest_view/common/guest_view_constants.h',
            'guest_view/common/guest_view_message_generator.cc',
            'guest_view/common/guest_view_message_generator.h',
            'guest_view/common/guest_view_messages.h',
          ]
        },
        {
          'target_name': 'guest_view_renderer',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content.gyp:content_common',
            '../content/content.gyp:content_renderer',
            '../third_party/WebKit/public/blink.gyp:blink',
            '../v8/src/v8.gyp:v8',
            'guest_view_common'
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'guest_view/renderer/guest_view_container_dispatcher.cc',
            'guest_view/renderer/guest_view_container_dispatcher.h',
            'guest_view/renderer/guest_view_container.cc',
            'guest_view/renderer/guest_view_container.h',
            'guest_view/renderer/guest_view_request.cc',
            'guest_view/renderer/guest_view_request.h',
            'guest_view/renderer/iframe_guest_view_container.cc',
            'guest_view/renderer/iframe_guest_view_container.h',
            'guest_view/renderer/iframe_guest_view_request.cc',
            'guest_view/renderer/iframe_guest_view_request.h',
          ],
        },
        {
          'target_name': 'guest_view_test_support',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content_shell_and_tests.gyp:test_support_content',
            'guest_view_browser',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'guest_view/browser/test_guest_view_manager.h',
            'guest_view/browser/test_guest_view_manager.cc',
          ],
        }
      ],
        }]
      ]
}
