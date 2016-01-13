# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libspeex',
      'product_name': 'speex',
      'type': 'static_library',
      'sources': [
        'libspeex/arch.h',
        'libspeex/bits.c',
        'libspeex/cb_search.c',
        'libspeex/cb_search.h',
        'libspeex/exc_10_16_table.c',
        'libspeex/exc_10_32_table.c',
        'libspeex/exc_20_32_table.c',
        'libspeex/exc_5_256_table.c',
        'libspeex/exc_5_64_table.c',
        'libspeex/exc_8_128_table.c',
        'libspeex/filters.c',
        'libspeex/filters.h',
        'libspeex/gain_table.c',
        'libspeex/gain_table_lbr.c',
        'libspeex/hexc_10_32_table.c',
        'libspeex/hexc_table.c',
        'libspeex/high_lsp_tables.c',
        'libspeex/lpc.c',
        'libspeex/lpc.h',
        'libspeex/lsp.c',
        'libspeex/lsp.h',
        'libspeex/lsp_tables_nb.c',
        'libspeex/ltp.c',
        'libspeex/ltp.h',
        'libspeex/math_approx.h',
        'libspeex/modes.c',
        'libspeex/modes.h',
        'libspeex/modes_wb.c',
        'libspeex/nb_celp.c',
        'libspeex/nb_celp.h',
        'libspeex/os_support.h',
        'libspeex/pseudofloat.h',
        'libspeex/quant_lsp.c',
        'libspeex/quant_lsp.h',
        'libspeex/sb_celp.c',
        'libspeex/sb_celp.h',
        'libspeex/speex.c',
        'libspeex/speex_callbacks.c',
        'libspeex/speex_header.c',
        'libspeex/stack_alloc.h',
        'libspeex/stereo.c',
        'libspeex/vbr.c',
        'libspeex/vbr.h',
        'libspeex/vq.c',
        'libspeex/vq.h',
        'libspeex/window.c',
        'include/speex/speex.h',
        'include/speex/speex_bits.h',
        'include/speex/speex_buffer.h',
        'include/speex/speex_callbacks.h',
        'include/speex/speex_echo.h',
        'include/speex/speex_header.h',
        'include/speex/speex_jitter.h',
        'include/speex/speex_preprocess.h',
        'include/speex/speex_resampler.h',
        'include/speex/speex_stereo.h',
        'include/speex/speex_types.h',
      ],
      'defines': [
        'FLOATING_POINT',
        'EXPORT=',
        'USE_ALLOCA',
        'inline=__inline',
      ],
      'include_dirs': [
        'include',
        'libspeex',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',  # Clients expect <speex/speex.h> to be a system header.
        ],
      },
      'msvs_disabled_warnings': [4244, 4305],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
