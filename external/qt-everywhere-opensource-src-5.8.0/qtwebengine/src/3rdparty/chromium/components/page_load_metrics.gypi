# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS != "ios"', {
      'targets': [
        {
          'target_name': 'page_load_metrics_common',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../ipc/ipc.gyp:ipc',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'page_load_metrics/common/page_load_metrics_messages.cc',
            'page_load_metrics/common/page_load_metrics_messages.h',
            'page_load_metrics/common/page_load_timing.cc',
            'page_load_metrics/common/page_load_timing.h',
          ],
        },
        {
          'target_name': 'page_load_metrics_browser',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content.gyp:content_browser',
            '../ipc/ipc.gyp:ipc',
            'page_load_metrics_common',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'page_load_metrics/browser/metrics_web_contents_observer.cc',
            'page_load_metrics/browser/metrics_web_contents_observer.h',
            'page_load_metrics/browser/page_load_metrics_observer.cc',
            'page_load_metrics/browser/page_load_metrics_observer.h',
            'page_load_metrics/browser/page_load_metrics_util.cc',
            'page_load_metrics/browser/page_load_metrics_util.h',
          ],
        },
        {
          'target_name': 'page_load_metrics_renderer',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content.gyp:content_renderer',
            '../third_party/WebKit/public/blink.gyp:blink',
            '../url/url.gyp:url_lib',
            'page_load_metrics_common',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'page_load_metrics/renderer/metrics_render_frame_observer.cc',
            'page_load_metrics/renderer/metrics_render_frame_observer.h',
            'page_load_metrics/renderer/page_timing_metrics_sender.cc',
            'page_load_metrics/renderer/page_timing_metrics_sender.h',
          ],
        },
      ],
    }]
  ]
}
