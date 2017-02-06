# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/web_cache/public/interfaces
      'target_name': 'web_cache_mojo_bindings',
      'type': 'static_library',
      'sources': [
        # NOTE: Sources duplicated in //components/web_cache/public/interfaces/BUILD.gn
        'web_cache/public/interfaces/web_cache.mojom',
      ],
      'includes': [ '../mojo/mojom_bindings_generator.gypi'],
    },
    {
      'target_name': 'web_cache_browser',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/content/content.gyp:content_browser',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        'web_cache_mojo_bindings',
      ],
      'sources': [
        'web_cache/browser/web_cache_manager.cc',
        'web_cache/browser/web_cache_manager.h',
      ],
      # Disable c4267 warnings until we fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      'target_name': 'web_cache_renderer',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/content/content.gyp:content_renderer',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        'web_cache_mojo_bindings',
      ],
      'sources': [
        'web_cache/renderer/web_cache_impl.cc',
        'web_cache/renderer/web_cache_impl.h',
      ],
    },
  ],
}
