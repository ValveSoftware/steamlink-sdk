# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'renderer_context_menu',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../components/components.gyp:search_engines',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'renderer_context_menu/context_menu_content_type.cc',
        'renderer_context_menu/context_menu_content_type.h',
        'renderer_context_menu/context_menu_delegate.cc',
        'renderer_context_menu/context_menu_delegate.h',
        'renderer_context_menu/render_view_context_menu_base.cc',
        'renderer_context_menu/render_view_context_menu_base.h',
        'renderer_context_menu/render_view_context_menu_observer.cc',
        'renderer_context_menu/render_view_context_menu_observer.h',
        'renderer_context_menu/render_view_context_menu_proxy.h',
        'renderer_context_menu/views/toolkit_delegate_views.cc',
        'renderer_context_menu/views/toolkit_delegate_views.h',
      ],
    },
  ],
}
