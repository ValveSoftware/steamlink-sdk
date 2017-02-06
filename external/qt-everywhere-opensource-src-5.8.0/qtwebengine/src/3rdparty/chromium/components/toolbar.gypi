# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/toolbar
      'target_name': 'toolbar',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_vector_icons',
        '../url/url.gyp:url_lib',
        'components_resources.gyp:components_resources',
        'components_strings.gyp:components_strings',
        'google_core_browser',
        'prefs/prefs.gyp:prefs',
        'security_state',
        'url_formatter/url_formatter.gyp:url_formatter',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'toolbar/toolbar_model.cc',
        'toolbar/toolbar_model.h',
        'toolbar/toolbar_model_delegate.h',
        'toolbar/toolbar_model_impl.cc',
        'toolbar/toolbar_model_impl.h',
      ],
    },
    {
      # GN version: //components/toolbar:test_support
      'target_name': 'toolbar_test_support',
      'type': 'static_library',
      'dependencies': [
        '../ui/gfx/gfx.gyp:gfx_vector_icons',
        'components_resources.gyp:components_resources',
        'toolbar',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'toolbar/test_toolbar_model.cc',
        'toolbar/test_toolbar_model.h',
      ],
      'conditions': [
        ['toolkit_views==1', {
          # Needed to get the TOOLKIT_VIEWS define.
          'dependencies': [
            '<(DEPTH)/ui/views/views.gyp:views',
          ],
        }],
      ],
    },
  ],
}
