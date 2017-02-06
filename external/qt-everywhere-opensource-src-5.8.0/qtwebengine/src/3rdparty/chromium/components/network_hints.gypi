# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/network_hints/common
      'target_name': 'network_hints_common',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/accessibility/accessibility.gyp:accessibility',
        '../url/ipc/url_ipc.gyp:url_ipc',
      ],
      'sources': [
        'network_hints/common/network_hints_common.cc',
        'network_hints/common/network_hints_common.h',
        'network_hints/common/network_hints_message_generator.cc',
        'network_hints/common/network_hints_message_generator.h',
        'network_hints/common/network_hints_messages.cc',
        'network_hints/common/network_hints_messages.h',
      ],
    },
    {
      # GN version: //components/network_hints/browser
      'target_name': 'network_hints_browser',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../content/content.gyp:content_browser',
        '../net/net.gyp:net',
      ],
      'sources': [
        'network_hints/browser/network_hints_message_filter.cc',
        'network_hints/browser/network_hints_message_filter.h',
      ],
    },
  ],
  'conditions': [
    ['OS!="ios"', {
      'targets': [
        {
          # GN version: //components/network_hints/renderer
          'target_name': 'network_hints_renderer',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            'network_hints_common',
            '../content/content.gyp:content_renderer',
            '../third_party/WebKit/public/blink.gyp:blink',
          ],
          'sources': [
            'network_hints/renderer/dns_prefetch_queue.cc',
            'network_hints/renderer/dns_prefetch_queue.h',
            'network_hints/renderer/prescient_networking_dispatcher.cc',
            'network_hints/renderer/prescient_networking_dispatcher.h',
            'network_hints/renderer/renderer_dns_prefetch.cc',
            'network_hints/renderer/renderer_dns_prefetch.h',
            'network_hints/renderer/renderer_preconnect.cc',
            'network_hints/renderer/renderer_preconnect.h',
          ],
        },
      ],
    }],
  ],
}
