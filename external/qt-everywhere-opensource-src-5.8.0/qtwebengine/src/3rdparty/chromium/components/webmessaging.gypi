# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'mojom_files': [
      'webmessaging/public/interfaces/broadcast_channel.mojom',
    ],
  },
  'targets': [
    {
      # GN version: //components/webmessaging/webmessaging
      'target_name': 'webmessaging',
      'type': 'static_library',
      'dependencies': [
        'webmessaging_mojo_bindings',
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
      ],
      'sources': [
        'webmessaging/broadcast_channel_provider.cc',
        'webmessaging/broadcast_channel_provider.h',
      ],
    },
    {
      'target_name': 'webmessaging_mojo_bindings',
      'type': 'static_library',
      'sources': [ '<@(mojom_files)' ],
      'dependencies': [
        '../url/url.gyp:url_mojom',
      ],
      'export_dependent_settings': [
        '../url/url.gyp:url_mojom',
      ],
      'variables': {
        'mojom_typemaps': [
          '../url/mojo/gurl.typemap',
          '../url/mojo/origin.typemap',
        ],
      },
      'includes': [
        '../mojo/mojom_bindings_generator.gypi',
      ],
    },
    {
      'target_name': 'webmessaging_mojo_bindings_for_blink',
      'type': 'static_library',
      'sources': [ '<@(mojom_files)' ],
      'dependencies': [
        '../url/url.gyp:url_mojom_for_blink',
      ],
      'export_dependent_settings': [
        '../url/url.gyp:url_mojom_for_blink',
      ],
      'variables': {
        'for_blink': 'true',
        'mojom_typemaps': [
          '../third_party/WebKit/Source/platform/mojo/KURL.typemap',
          '../third_party/WebKit/Source/platform/mojo/SecurityOrigin.typemap',
        ],
      },
      'includes': [
        '../mojo/mojom_bindings_generator.gypi',
      ],
    },
  ],
}
