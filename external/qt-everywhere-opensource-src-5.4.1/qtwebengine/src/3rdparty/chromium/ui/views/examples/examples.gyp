# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'views_examples_lib',
      'type': '<(component)',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../skia/skia.gyp:skia',
        '../../../third_party/icu/icu.gyp:icui18n',
        '../../../third_party/icu/icu.gyp:icuuc',
        '../../base/ui_base.gyp:ui_base',
        '../../events/events.gyp:events',
        '../../gfx/gfx.gyp:gfx',
        '../../gfx/gfx.gyp:gfx_geometry',
        '../../resources/ui_resources.gyp:ui_resources',
        '../../resources/ui_resources.gyp:ui_test_pak',
        '../views.gyp:views',
      ],
      'include_dirs': [
        '../../..',
      ],
      'defines': [
        'VIEWS_EXAMPLES_IMPLEMENTATION',
      ],
      'sources': [
        'bubble_example.cc',
        'bubble_example.h',
        'button_example.cc',
        'button_example.h',
        'checkbox_example.cc',
        'checkbox_example.h',
        'combobox_example.cc',
        'combobox_example.h',
        'double_split_view_example.cc',
        'double_split_view_example.h',
        'example_base.cc',
        'example_base.h',
        'example_combobox_model.cc',
        'example_combobox_model.h',
        'examples_window.cc',
        'examples_window.h',
        'label_example.cc',
        'label_example.h',
        'link_example.cc',
        'link_example.h',
        'message_box_example.cc',
        'message_box_example.h',
        'menu_example.cc',
        'menu_example.h',
        'multiline_example.cc',
        'multiline_example.h',
        'progress_bar_example.cc',
        'progress_bar_example.h',
        'radio_button_example.cc',
        'radio_button_example.h',
        'scroll_view_example.cc',
        'scroll_view_example.h',
        'single_split_view_example.cc',
        'single_split_view_example.h',
        'slider_example.cc',
        'slider_example.h',
        'tabbed_pane_example.cc',
        'tabbed_pane_example.h',
        'table_example.cc',
        'table_example.h',
        'text_example.cc',
        'text_example.h',
        'textfield_example.cc',
        'textfield_example.h',
        'throbber_example.cc',
        'throbber_example.h',
        'tree_view_example.cc',
        'tree_view_example.h',
        'views_examples_export.h',
        'widget_example.cc',
        'widget_example.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'include_dirs': [
            '../../../third_party/wtl/include',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        }],
        ['use_aura==1', {
          'dependencies': [
            '../../aura/aura.gyp:aura',
          ],
        }],
      ],
    },  # target_name: views_examples_lib
    {
      'target_name': 'views_examples_exe',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../base/base.gyp:base_i18n',
        '../../base/ui_base.gyp:ui_base',
        '../../compositor/compositor.gyp:compositor',
        '../../compositor/compositor.gyp:compositor_test_support',
        '../../gfx/gfx.gyp:gfx',
        '../../resources/ui_resources.gyp:ui_test_pak',
        '../views.gyp:views',
        '../views.gyp:views_test_support',
        'views_examples_lib',
      ],
      'sources': [
        'examples_main.cc',
      ],
      'conditions': [
        ['use_aura==1', {
          'dependencies': [
            '../../aura/aura.gyp:aura',
          ],
        }],
      ],
    },  # target_name: views_examples_exe
    {
      'target_name': 'views_examples_with_content_lib',
      'type': '<(component)',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../content/content.gyp:content',
        '../../../skia/skia.gyp:skia',
        '../../../url/url.gyp:url_lib',
        '../../events/events.gyp:events',
        '../controls/webview/webview.gyp:webview',
        '../views.gyp:views',
        'views_examples_lib',
      ],
      'defines': [
        'VIEWS_EXAMPLES_WITH_CONTENT_IMPLEMENTATION',
      ],
      'sources': [
        'examples_window_with_content.cc',
        'examples_window_with_content.h',
        'views_examples_with_content_export.h',
        'webview_example.cc',
        'webview_example.h',
      ],
    },  # target_name: views_examples_with_content_lib
    {
      'target_name': 'views_examples_with_content_exe',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../content/content.gyp:content',
        '../../views_content_client/views_content_client.gyp:views_content_client',
        'views_examples_with_content_lib',
      ],
      'sources': [
        '../../../content/app/startup_helper_win.cc',
        'examples_with_content_main_exe.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-limm32.lib',
              '-loleacc.lib',
            ]
          },
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': [
                'views_examples.exe.manifest',
              ],
            },
            'VCLinkerTool': {
              'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
            },
          },
          'dependencies': [
            '../../../sandbox/sandbox.gyp:sandbox',
          ],
        }],
      ],
    },  # target_name: views_examples_with_content_exe
  ],
}
