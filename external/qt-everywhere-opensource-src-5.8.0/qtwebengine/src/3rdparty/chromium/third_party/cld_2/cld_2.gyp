# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# MAINTAINERS:
# See the BUILD.gn file for more extensive comments and documentation. This
# .gyp file exists only for compatibility with gyp. The variables defined below
# are used in equivalent targets in BUILD.GN.

{
  'variables': {
    # These sources need to be included in both static and dynamic builds as
    # well as the dynamic data tool.
    'cld2_core_sources': [
      'src/internal/cld2tablesummary.h',
      'src/internal/cldutil.h',
      'src/internal/cldutil_shared.h',
      'src/internal/compact_lang_det_hint_code.h',
      'src/internal/compact_lang_det_impl.h',
      'src/internal/debug.h',
      'src/internal/fixunicodevalue.h',
      'src/internal/generated_language.h',
      'src/internal/generated_ulscript.h',
      'src/internal/getonescriptspan.h',
      'src/internal/integral_types.h',
      'src/internal/lang_script.h',
      'src/internal/langspan.h',
      'src/internal/offsetmap.h',
      'src/internal/port.h',
      'src/internal/scoreonescriptspan.h',
      'src/internal/stringpiece.h',
      'src/internal/tote.h',
      'src/internal/utf8prop_lettermarkscriptnum.h',
      'src/internal/utf8repl_lettermarklower.h',
      'src/internal/utf8scannot_lettermarkspecial.h',
      'src/internal/utf8statetable.h',
      'src/public/compact_lang_det.h',
      'src/public/encodings.h',
    ],

    # These sources may have different compilation results based on flags.
    'cld2_core_impl_sources': [
      'src/internal/cldutil.cc',
      'src/internal/cldutil_shared.cc',
      'src/internal/compact_lang_det.cc',
      'src/internal/compact_lang_det_hint_code.cc',
      'src/internal/compact_lang_det_impl.cc',
      'src/internal/debug_empty.cc',
      'src/internal/fixunicodevalue.cc',
      'src/internal/generated_entities.cc',
      'src/internal/generated_language.cc',
      'src/internal/generated_ulscript.cc',
      'src/internal/getonescriptspan.cc',
      'src/internal/lang_script.cc',
      'src/internal/offsetmap.cc',
      'src/internal/scoreonescriptspan.cc',
      'src/internal/tote.cc',
      'src/internal/utf8statetable.cc',
    ],

    # These sources are for both small-table and large-table data sets.
    'cld2_data_sources': [
      'src/internal/cld2_generated_cjk_compatible.cc',
      'src/internal/cld2_generated_deltaoctachrome.cc',
      'src/internal/cld2_generated_distinctoctachrome.cc',
      'src/internal/cld_generated_cjk_delta_bi_4.cc',
      'src/internal/cld_generated_cjk_uni_prop_80.cc',
      'src/internal/cld_generated_score_quad_octa_2.cc',
      'src/internal/generated_distinct_bi_0.cc',
    ],

    # Used when cld2_table_size == 0 (small tables)
    # See 'cld2_table_size' in build/common.gypi for more information.
    'cld2_data_smallest_sources': [
      'src/internal/cld2_generated_quadchrome_16.cc'
    ],

    # Used when cld2_table_size == 2 (large tables)
    # See 'cld2_table_size' in build/common.gypi for more information.
    'cld2_data_largest_sources': [
      'src/internal/cld2_generated_quadchrome_2.cc'
    ],
  },

  'targets': [
    {
      # GN version: //third_party/cld_2
      'target_name': 'cld_2',
      'type': 'static_library',
      'include_dirs': [
        'src/internal',
        'src/public',
      ],
      'sources': [
        '<@(cld2_core_sources)',
        '<@(cld2_core_impl_sources)',
        '<@(cld2_data_sources)',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_disabled_warnings': [4267], # size_t -> int conversion.
        }],
        ['cld2_table_size==0', {
          'sources+': ['<@(cld2_data_smallest_sources)']
        }],
        ['cld2_table_size==2', {
          'sources+': ['<@(cld2_data_largest_sources)']
        }],
      ],
      'variables': {
        'clang_warning_flags': [
          # The generated files don't have braces around subobject initializers.
          '-Wno-missing-braces',
          # cld_2 contains unused private fields,
          # https://code.google.com/p/cld2/issues/detail?id=37
          '-Wno-unused-private-field',
        ],
      },
    },
  ],
}
