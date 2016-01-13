# This file is generated. Do not edit.
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libvpx_intrinsics_neon',
      'type': 'static_library',
      'include_dirs': [
        'source/config/<(OS_CATEGORY)/<(target_arch_full)',
        '<(libvpx_source)',
      ],
      'sources': [
        '<(libvpx_source)/vp8/common/arm/neon/bilinearpredict_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/copymem_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/dc_only_idct_add_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/dequant_idct_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/dequantizeb_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/idct_blk_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/iwalsh_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/loopfiltersimplehorizontaledge_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/mbloopfilter_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/sad_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/shortidct4x4llm_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/sixtappredict_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/variance_neon.c',
        '<(libvpx_source)/vp8/encoder/arm/neon/denoising_neon.c',
        '<(libvpx_source)/vp9/common/arm/neon/vp9_convolve_neon.c',
        '<(libvpx_source)/vp9/common/arm/neon/vp9_idct16x16_neon.c',
        '<(libvpx_source)/vp9/common/arm/neon/vp9_loopfilter_16_neon.c',
      ],
      'conditions': [
        ['os_posix==1 and OS!="mac" and OS!="ios"', {
          'cflags!': [ '-mfpu=vfpv3-d16' ],
          'cflags': [ '-mfpu=neon', ],
        }],
        ['OS=="mac" or OS=="ios"', {
          'xcode_settings': {
            'OTHER_CFLAGS': [ '-mfpu=neon', ],
          },
        }],
      ],
    },
  ],
}
