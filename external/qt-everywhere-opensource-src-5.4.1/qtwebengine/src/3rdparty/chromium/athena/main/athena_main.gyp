# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'athena_main',
      'type': 'executable',
      'dependencies': [
        '../athena.gyp:athena_lib',
        '../athena.gyp:athena_content_lib',
        '../../apps/shell/app_shell.gyp:app_shell_lib',
        '../../skia/skia.gyp:skia',
        '../../ui/accessibility/accessibility.gyp:ax_gen',
        '../../ui/app_list/app_list.gyp:app_list',
        '../../ui/keyboard/keyboard.gyp:keyboard',
        '../../ui/views/views.gyp:views',
        '../../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'athena_app_window_controller.cc',
        'athena_app_window_controller.h',
        'athena_launcher.cc',
        'athena_launcher.h',
        'url_search_provider.cc',
        'url_search_provider.h',
        'athena_main.cc',
        'placeholder.cc',
        'placeholder.h',
      ],
    },
    {
      'target_name': 'athena_shell',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../skia/skia.gyp:skia',
        '../../ui/accessibility/accessibility.gyp:ax_gen',
        '../../ui/aura/aura.gyp:aura',
        '../../ui/compositor/compositor.gyp:compositor_test_support',
        '../../ui/gfx/gfx.gyp:gfx',
        '../../ui/resources/ui_resources.gyp:ui_test_pak',
        '../athena.gyp:athena_lib',
        '../athena.gyp:athena_test_support',
      ],
      'sources': [
        'athena_shell.cc',
      ],
    }
  ],  # targets
}
