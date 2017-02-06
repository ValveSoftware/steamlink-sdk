# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/app_list/presenter
      'target_name': 'app_list_presenter',
      'type': '<(component)',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../skia/skia.gyp:skia',
        '../../aura/aura.gyp:aura',
        '../../compositor/compositor.gyp:compositor',
        '../../events/events.gyp:events_base',
        '../../events/events.gyp:events',
        '../../gfx/gfx.gyp:gfx_geometry',
        '../../views/views.gyp:views',
        '../app_list.gyp:app_list',

        # Temporary dependency to fix compile flake in http://crbug.com/611898.
        # TODO(tapted): Remove once http://crbug.com/612382 is fixed.
        '../../accessibility/accessibility.gyp:ax_gen',
      ],
      'defines': [
        'APP_LIST_PRESENTER_IMPLEMENTATION',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'app_list_presenter.h',
        'app_list_presenter_delegate.cc',
        'app_list_presenter_delegate.h',
        'app_list_presenter_delegate_factory.h',
        'app_list_presenter_export.h',
        'app_list_presenter_impl.cc',
        'app_list_presenter_impl.h',
        'app_list_view_delegate_factory.cc',
        'app_list_view_delegate_factory.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # GN version: //ui/app_list/presenter:test_support
      'target_name': 'app_list_presenter_test_support',
      'type': 'static_library',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../skia/skia.gyp:skia',
        'app_list_presenter',

        # Temporary dependency to fix compile flake in http://crbug.com/611898.
        # TODO(tapted): Remove once http://crbug.com/612382 is fixed.
        '../../accessibility/accessibility.gyp:ax_gen',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'test/app_list_presenter_impl_test_api.cc',
        'test/app_list_presenter_impl_test_api.h',
      ],
    },
    {
      # GN version: //ui/app_list/presenter:app_list_presenter_unittests
      'target_name': 'app_list_presenter_unittests',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../base/base.gyp:test_support_base',
        '../../../skia/skia.gyp:skia',
        '../../../testing/gtest.gyp:gtest',
        '../../aura/aura.gyp:aura_test_support',
        '../../resources/ui_resources.gyp:ui_test_pak',
        '../../views/views.gyp:views',
        '../../wm/wm.gyp:wm',
        '../app_list.gyp:app_list_test_support',
        'app_list_presenter',
        'app_list_presenter_test_support',

        # Temporary dependency to fix compile flake in http://crbug.com/611898.
        # TODO(tapted): Remove once http://crbug.com/612382 is fixed.
        '../../accessibility/accessibility.gyp:ax_gen',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'app_list_presenter_impl_unittest.cc',
        'test/run_all_unittests.cc',
      ],
      # Disable c4267 warnings until we fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'app_list_presenter_unittests_run',
          'type': 'none',
          'dependencies': [
            'app_list_presenter_unittests',
          ],
          'includes': [
            '../../../build/isolate.gypi',
          ],
          'sources': [
            'app_list_presenter_unittests.isolate',
          ],
          'conditions': [
            ['use_x11 == 1', {
              'dependencies': [
                '../../../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
