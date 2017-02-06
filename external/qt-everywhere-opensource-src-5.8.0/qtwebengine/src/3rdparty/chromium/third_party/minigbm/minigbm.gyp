# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_system_minigbm%': 0,
  },
  'conditions': [
    ['use_system_minigbm==0', {
      'targets': [
        {
          'target_name': 'minigbm',
          'type': 'shared_library',
          'dependencies' : [
            '../../build/linux/system.gyp:libdrm',
          ],
          'sources': [
            'src/cirrus.c',
            'src/evdi.c',
            'src/exynos.c',
            'src/gbm.c',
            'src/gma500.c',
            'src/helpers.c',
            'src/i915.c',
            'src/marvell.c',
            'src/mediatek.c',
            'src/rockchip.c',
            'src/tegra.c',
            'src/udl.c',
            'src/virtio_gpu.c',
          ],
          'include_dirs': [
            'src',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
               'src',
            ],
          },
        },
      ],
    }, { # 'use_system_minigbm!=0
      'targets': [
        {
          'target_name': 'minigbm',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gbm)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other gbm)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l gbm)',
            ],
          },
        },
      ],
    }],
  ],
}
