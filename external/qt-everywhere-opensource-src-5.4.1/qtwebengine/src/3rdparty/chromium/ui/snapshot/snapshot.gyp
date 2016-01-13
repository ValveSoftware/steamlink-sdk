# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'snapshot',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
        '../base/ui_base.gyp:ui_base',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
      ],
      'defines': [
        'SNAPSHOT_IMPLEMENTATION',
      ],
      'sources': [
        'snapshot.h',
        'snapshot_android.cc',
        'snapshot_async.cc',
        'snapshot_async.h',
        'snapshot_aura.cc',
        'snapshot_export.h',
        'snapshot_ios.mm',
        'snapshot_mac.mm',
        'snapshot_win.cc',
        'snapshot_win.h',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['use_aura==1 or OS=="android"', {
          'dependencies': [
            '../../cc/cc.gyp:cc',
            '../../gpu/gpu.gyp:command_buffer_common',
          ],
        }],
        ['use_aura!=1 and OS!="android"', {
	  'sources!': [
            'snapshot_async.cc',
            'snapshot_async.h',
          ],
        }],
        ['use_aura==1', {
          'dependencies': [
            '../aura/aura.gyp:aura',
            '../compositor/compositor.gyp:compositor',
          ],
        }],
      ],
    },
    {
      'target_name': 'snapshot_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../../skia/skia.gyp:skia',
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../testing/gtest.gyp:gtest',
        '../base/ui_base.gyp:ui_base',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        'snapshot'
      ],
      'sources': [
        'snapshot_aura_unittest.cc',
        'snapshot_mac_unittest.mm',
        'test/run_all_unittests.cc',
      ],
      'conditions': [
        ['use_aura==1', {
          'dependencies': [
            '../../base/base.gyp:test_support_base',
            '../aura/aura.gyp:aura_test_support',
            '../compositor/compositor.gyp:compositor',
            '../compositor/compositor.gyp:compositor_test_support',
            '../wm/wm.gyp:wm',
          ],
        }],
        # See http://crbug.com/162998#c4 for why this is needed.
        ['OS=="linux" and use_allocator!="none"', {
          'dependencies': [
            '../../base/allocator/allocator.gyp:allocator',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'snapshot_test_support',
          'type': 'static_library',
          'sources': [
            'test/snapshot_desktop.h',
            'test/snapshot_desktop_win.cc',
          ],
          'dependencies': [
            'snapshot',
          ],
          'include_dirs': [
            '../..',
          ],
        },
      ],
    }],
  ],
}
