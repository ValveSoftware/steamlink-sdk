# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'external_ozone_platforms': [],
    'external_ozone_platform_files': [],
    'external_ozone_platform_deps': [],
    'external_ozone_platform_unittest_deps': [],
    'internal_ozone_platforms': [],
    'internal_ozone_platform_deps': [],
    'internal_ozone_platform_unittest_deps': [],
  },
  'targets': [
    {
      'target_name': 'ozone_base',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'defines': [
        'OZONE_BASE_IMPLEMENTATION',
      ],
      'sources': [
        'public/cursor_factory_ozone.cc',
        'public/cursor_factory_ozone.h',
        'public/event_factory_ozone.cc',
        'public/event_factory_ozone.h',
        'public/gpu_platform_support.cc',
        'public/gpu_platform_support.h',
        'public/gpu_platform_support_host.cc',
        'public/gpu_platform_support_host.h',
        'public/overlay_candidates_ozone.cc',
        'public/overlay_candidates_ozone.h',
        'public/surface_factory_ozone.cc',
        'public/surface_factory_ozone.h',
        'public/surface_ozone_canvas.h',
        'public/surface_ozone_egl.h',
      ],
    },
    {
      'target_name': 'ozone',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/ui/events/events.gyp:events',
        '<(DEPTH)/ui/events/ozone/events_ozone.gyp:events_ozone',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<@(external_ozone_platform_deps)',
        '<@(internal_ozone_platform_deps)',
        'ozone_base',
      ],
      'defines': [
        'OZONE_IMPLEMENTATION',
      ],
      'variables': {
        'platform_list_cc_file': '<(INTERMEDIATE_DIR)/ui/ozone/platform_list.cc',
        'platform_list_h_file': '<(SHARED_INTERMEDIATE_DIR)/ui/ozone/platform_list.h',
        'platform_list_txt_file': '<(SHARED_INTERMEDIATE_DIR)/ui/ozone/platform_list.txt',
        'constructor_list_cc_file': '<(INTERMEDIATE_DIR)/ui/ozone/constructor_list.cc',
        'ozone_platforms': [
          '<@(external_ozone_platforms)',
          '<@(internal_ozone_platforms)',
        ],
      },
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'sources': [
        '<(platform_list_cc_file)',
        '<(platform_list_h_file)',
        '<(constructor_list_cc_file)',

        # common/chromeos files are excluded automatically when building with
        # chromeos=0, by exclusion rules in filename_rules.gypi due to the
        # 'chromeos' folder name.
        'common/chromeos/native_display_delegate_ozone.cc',
        'common/chromeos/native_display_delegate_ozone.h',
        'common/chromeos/touchscreen_device_manager_ozone.cc',
        'common/chromeos/touchscreen_device_manager_ozone.h',
        'ozone_platform.cc',
        'ozone_platform.h',
        'ozone_switches.cc',
        'ozone_switches.h',
        'platform_selection.cc',
        'platform_selection.h',
        '<@(external_ozone_platform_files)',
      ],
      'actions': [
        {
          'action_name': 'generate_ozone_platform_list',
          'variables': {
            'generator_path': 'generate_ozone_platform_list.py',
          },
          'inputs': [
            '<(generator_path)',
          ],
          'outputs': [
            '<(platform_list_cc_file)',
            '<(platform_list_h_file)',
            '<(platform_list_txt_file)',
          ],
          'action': [
            'python',
            '<(generator_path)',
            '--output_cc=<(platform_list_cc_file)',
            '--output_h=<(platform_list_h_file)',
            '--output_txt=<(platform_list_txt_file)',
            '--default=<(ozone_platform)',
            '<@(ozone_platforms)',
          ],
        },
        {
          'action_name': 'generate_constructor_list',
          'variables': {
            'generator_path': 'generate_constructor_list.py',
          },
          'inputs': [
            '<(generator_path)',
            '<(platform_list_txt_file)',
          ],
          'outputs': [
            '<(constructor_list_cc_file)',
          ],
          'action': [
            'python',
            '<(generator_path)',
            '--platform_list=<(platform_list_txt_file)',
            '--output_cc=<(constructor_list_cc_file)',
            '--namespace=ui',
            '--typename=OzonePlatform',
            '--include="ui/ozone/ozone_platform.h"'
          ],
        },
      ],
      'conditions': [
        ['use_udev == 0', {
          'sources/': [
            ['exclude', '_udev\\.(h|cc)$'],
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '<(DEPTH)/ui/display/display.gyp:display_types',
          ],
        }],
      ],
    },
    {
      'target_name': 'ozone_unittests',
      'type': '<(gtest_target_type)',
      'sources': [
        'run_all_unittests.cc',
      ],
      'dependencies': [
        'ozone',
        'ozone_base',
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../testing/gtest.gyp:gtest',
        '<@(external_ozone_platform_unittest_deps)',
        '<@(internal_ozone_platform_unittest_deps)',
      ],
    },
  ],
  'conditions': [
    ['<(ozone_platform_caca) == 1', {
      'includes': [
        'platform/caca/caca.gypi',
      ],
    }],
    ['<(ozone_platform_dri) == 1 or <(ozone_platform_gbm) == 1', {
      'includes': [
        'platform/dri/dri.gypi',
      ],
    }],
    ['<(ozone_platform_egltest) == 1', {
      'includes': [
        'platform/egltest/egltest.gypi',
      ],
    }],
    ['<(ozone_platform_gbm) == 1', {
      'includes': [
        'platform/dri/gbm.gypi',
      ],
    }],
    ['<(ozone_platform_test) == 1', {
      'includes': [
        'platform/test/test.gypi',
      ],
    }],
  ],
}
