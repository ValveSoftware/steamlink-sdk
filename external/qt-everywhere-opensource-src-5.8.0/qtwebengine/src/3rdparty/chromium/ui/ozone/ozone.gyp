# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    # Normally, the external_ozone_platform* variables below can be
    # overridden by supplement.gypi which must exist exactly one level
    # below the top-level src directory. They can now also be set in
    # $GYP_DEFINES
    'external_ozone_platforms%': [],
    'external_ozone_platform_files%': [],
    'external_ozone_platform_deps%': [],
    'external_ozone_platform_unittest_deps%': [],
    'internal_ozone_platforms': [],
    'internal_ozone_platform_deps': [],
    'internal_ozone_platform_unittest_deps': [],

    # This enables memory-mapped access to accelerated graphics buffers via
    # the VGEM ("virtual GEM") driver. This is currently only available on
    # Chrome OS kernels and affects code in the GBM ozone platform.
    # TODO(dshwang): remove this flag when all gbm hardware supports vgem map.
    # crbug.com/519587
    'use_vgem_map%': 0,
  },
  'targets': [
    {
      # GN version: //ui/ozone:ozone_base
      'target_name': 'ozone_base',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/ipc/ipc.gyp:ipc',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/ui/display/display.gyp:display_types',
        '<(DEPTH)/ui/display/display.gyp:display_util',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/ui/gfx/ipc/geometry/gfx_ipc_geometry.gyp:gfx_ipc_geometry',
        '<(DEPTH)/ui/gfx/ipc/gfx_ipc.gyp:gfx_ipc',
        '<(DEPTH)/ui/gfx/ipc/skia/gfx_ipc_skia.gyp:gfx_ipc_skia',
      ],
      'defines': [
        'OZONE_BASE_IMPLEMENTATION',
      ],
      'sources': [
        'public/client_native_pixmap.h',
        'public/cursor_factory_ozone.cc',
        'public/cursor_factory_ozone.h',
        'public/gpu_platform_support.cc',
        'public/gpu_platform_support.h',
        'public/gpu_platform_support_host.cc',
        'public/gpu_platform_support_host.h',
        'public/input_controller.cc',
        'public/input_controller.h',
        'public/native_pixmap.h',
        'public/overlay_candidates_ozone.cc',
        'public/overlay_candidates_ozone.h',
        'public/overlay_manager_ozone.h',
        'public/ozone_switches.cc',
        'public/ozone_switches.h',
        'public/surface_factory_ozone.cc',
        'public/surface_factory_ozone.h',
        'public/surface_ozone_canvas.h',
        'public/surface_ozone_egl.cc',
        'public/surface_ozone_egl.h',
        'public/system_input_injector.h',
      ],
    },
    {
      # GN version: //ui/ozone/common
      'target_name': 'ozone_common',
      'type': 'static_library',
      'dependencies': [
        'ozone_base',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/ipc/ipc.gyp:ipc',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/ui/display/display.gyp:display_types',
        '<(DEPTH)/ui/display/display.gyp:display_util',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/ui/gfx/ipc/geometry/gfx_ipc_geometry.gyp:gfx_ipc_geometry',
        '<(DEPTH)/ui/gfx/ipc/skia/gfx_ipc_skia.gyp:gfx_ipc_skia',
      ],
      'sources': [
        'common/display_mode_proxy.cc',
        'common/display_mode_proxy.h',
        'common/display_snapshot_proxy.cc',
        'common/display_snapshot_proxy.h',
        'common/display_util.cc',
        'common/display_util.h',
        'common/egl_util.cc',
        'common/egl_util.h',
        'common/gpu/ozone_gpu_message_generator.cc',
        'common/gpu/ozone_gpu_message_generator.h',
        'common/gpu/ozone_gpu_message_params.cc',
        'common/gpu/ozone_gpu_message_params.h',
        'common/gpu/ozone_gpu_messages.h',
        'common/native_display_delegate_ozone.cc',
        'common/native_display_delegate_ozone.h',
        'common/stub_overlay_manager.cc',
        'common/stub_overlay_manager.h',
      ],
    },
    {
      # GN version: //ui/ozone:platform
      'target_name': 'ozone_platform',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/ipc/ipc.gyp:ipc',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/ui/display/display.gyp:display_types',
        '<(DEPTH)/ui/display/display.gyp:display_util',
        '<(DEPTH)/ui/events/events.gyp:events',
        '<(DEPTH)/ui/events/devices/events_devices.gyp:events_devices',
        '<(DEPTH)/ui/events/ozone/events_ozone.gyp:events_ozone',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/ui/gfx/ipc/gfx_ipc.gyp:gfx_ipc',
        '<(DEPTH)/ui/gfx/ipc/skia/gfx_ipc_skia.gyp:gfx_ipc_skia',
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

        'common/stub_client_native_pixmap_factory.cc',
        'common/stub_client_native_pixmap_factory.h',
        'platform_selection.cc',
        'platform_selection.h',
        'public/client_native_pixmap_factory.cc',
        'public/client_native_pixmap_factory.h',
        'public/ozone_gpu_test_helper.cc',
        'public/ozone_gpu_test_helper.h',
        'public/ozone_platform.cc',
        'public/ozone_platform.h',
        '<@(external_ozone_platform_files)',
      ],
      'actions': [
        {
          # GN version: //ui/ozone:generate_ozone_platform_list
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
          # GN version: //ui/ozone:generate_constructor_list
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
            '--typename=ClientNativePixmapFactory',
            '--include="ui/ozone/public/ozone_platform.h"',
            '--include="ui/ozone/public/client_native_pixmap_factory.h"'
          ],
        },
      ],
      'conditions': [
        ['use_udev == 0', {
          'sources/': [
            ['exclude', '_udev\\.(h|cc)$'],
          ],
        }],
      ],
    },
    {
      # GN version: //ui/ozone
      'target_name': 'ozone',
      'type': '<(component)',
      'sources': [
        'empty.cc',
      ],
      'dependencies': [
        'ozone_platform',
      ],
    },
    {
      'target_name': 'ozone_unittests',
      'type': '<(gtest_target_type)',
      'sources': [
        'run_all_unittests.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../testing/gtest.gyp:gtest',
        '../gfx/gfx.gyp:gfx_geometry',
        '<@(external_ozone_platform_unittest_deps)',
        '<@(internal_ozone_platform_unittest_deps)',
      ],
    },
    {
      'target_name': 'vgem_map',
      'type': 'none',
      'conditions': [
        ['use_vgem_map==1', {
          'direct_dependent_settings': {
            'defines': [
              'USE_VGEM_MAP',
            ],
          },
        }],
      ],
    },
  ],
  'conditions': [
    ['<(ozone_platform_caca) == 1', {
      'includes': [
        'platform/caca/caca.gypi',
      ],
    }],
    ['<(ozone_platform_cast) == 1', {
      'includes': [
        'platform/cast/cast.gypi',
      ],
    }],
    ['<(ozone_platform_egltest) == 1', {
      'includes': [
        'platform/egltest/egltest.gypi',
      ],
    }],
    ['<(ozone_platform_gbm) == 1', {
      'includes': [
        'platform/drm/gbm.gypi',
      ],
    }],
    ['<(ozone_platform_headless) == 1', {
      'includes': [
        'platform/headless/headless.gypi',
      ],
    }],
    ['<(ozone_platform_wayland) == 1', {
      'includes': [
        'platform/wayland/wayland.gypi',
      ],
    }],
  ],
}
