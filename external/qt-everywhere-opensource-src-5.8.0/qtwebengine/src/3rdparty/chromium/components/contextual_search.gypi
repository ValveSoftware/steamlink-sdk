# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN: //components/contextual_search:browser
      'target_name': 'contextual_search_browser',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'contextual_search_mojo_bindings',
        '../base/base.gyp:base',
      ],
      'sources': [
        'contextual_search/browser/contextual_search_js_api_service_impl.cc',
        'contextual_search/browser/contextual_search_js_api_service_impl.h',
      ],
    },
    {
      # GN: //components/contextual_search:renderer
      'target_name': 'contextual_search_renderer',
      'type': 'static_library',
      'include_dirs': [
        '..',
        '../third_party/WebKit',
      ],
      'dependencies': [
        'contextual_search_mojo_bindings',
        '../base/base.gyp:base',
        '../content/content.gyp:content_common',
        '../third_party/WebKit/public/blink.gyp:blink',
      ],
      'sources': [
        'contextual_search/renderer/contextual_search_wrapper.cc',
        'contextual_search/renderer/contextual_search_wrapper.h',
        'contextual_search/renderer/overlay_js_render_frame_observer.cc',
        'contextual_search/renderer/overlay_js_render_frame_observer.h',
        'contextual_search/renderer/overlay_page_notifier_service_impl.cc',
        'contextual_search/renderer/overlay_page_notifier_service_impl.h',
      ],
    },
    {
      # GN version: //components/contextual_search:mojo_bindings
      'target_name': 'contextual_search_mojo_bindings',
      'type': 'static_library',
      'sources': [
        'contextual_search/common/contextual_search_js_api_service.mojom',
        'contextual_search/common/overlay_page_notifier_service.mojom',
      ],
      'includes': [
        '../mojo/mojom_bindings_generator.gypi',
      ],
    },
  ],
}
