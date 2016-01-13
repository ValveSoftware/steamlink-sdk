# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Note to maintainers: In the January 2014 release (*_0122*), there are some
# options for building:
# Pick one quadgram file (cld2_generated_quadchrome*.cc):
#   0122_16 = 160K entries, smallest size, lowest accuracy (set cld2_table_size=0)
#   0122_19 = 192K entries, medium size, medium accuracy (set cld2_table_size=1)
#   0122_2  = 256K entries, largest size, highest accuracy (set cld2_table_size=2)
#
# For the CJK bigram file (cld_generated_cjk_delta_bi*.cc), always use
# cld_generated_cjk_delta_bi_4.cc, as this is intended for use with Chromium.
# The _32 variant of the file is intended for applications that use the full
# 175-language version of CLD2.

{
  'targets': [
    {
      'target_name': 'cld_2_dynamic_data_tool',
      'type': 'executable',
      'include_dirs': [
        'src/internal',
        'src/public',
      ],
      'sources': [
        'src/internal/cld2_dynamic_data.h',
        'src/internal/cld2_dynamic_data.cc',
        'src/internal/cld2_dynamic_data_extractor.h',
        'src/internal/cld2_dynamic_data_extractor.cc',
        'src/internal/cld2_dynamic_data_loader.h',
        'src/internal/cld2_dynamic_data_loader.cc',
        'src/internal/cld2_dynamic_data_tool.cc',
        'src/internal/cld2_generated_cjk_compatible.cc',
        'src/internal/cld2_generated_deltaoctachrome0122.cc',
        'src/internal/cld2_generated_distinctoctachrome0122.cc',
        'src/internal/cld2tablesummary.h',
        'src/internal/cld_generated_cjk_delta_bi_4.cc',
        'src/internal/cld_generated_cjk_uni_prop_80.cc',
        'src/internal/cld_generated_score_quad_octa_0122_2.cc',
        'src/internal/cldutil.cc',
        'src/internal/cldutil.h',
        'src/internal/cldutil_shared.cc',
        'src/internal/cldutil_shared.h',
        'src/internal/compact_lang_det.cc',
        'src/internal/compact_lang_det_hint_code.cc',
        'src/internal/compact_lang_det_hint_code.h',
        'src/internal/compact_lang_det_impl.cc',
        'src/internal/compact_lang_det_impl.h',
        'src/internal/debug.h',
        'src/internal/debug_empty.cc',
        'src/internal/fixunicodevalue.cc',
        'src/internal/fixunicodevalue.h',
        'src/internal/generated_distinct_bi_0.cc',
        'src/internal/generated_entities.cc',
        'src/internal/generated_language.cc',
        'src/internal/generated_language.h',
        'src/internal/generated_ulscript.cc',
        'src/internal/generated_ulscript.h',
        'src/internal/getonescriptspan.cc',
        'src/internal/getonescriptspan.h',
        'src/internal/integral_types.h',
        'src/internal/lang_script.cc',
        'src/internal/lang_script.h',
        'src/internal/langspan.h',
        'src/internal/offsetmap.cc',
        'src/internal/offsetmap.h',
        'src/internal/port.h',
        'src/internal/scoreonescriptspan.cc',
        'src/internal/scoreonescriptspan.h',
        'src/internal/stringpiece.h',
        'src/internal/tote.cc',
        'src/internal/tote.h',
        'src/internal/utf8prop_lettermarkscriptnum.h',
        'src/internal/utf8repl_lettermarklower.h',
        'src/internal/utf8scannot_lettermarkspecial.h',
        'src/internal/utf8statetable.cc',
        'src/internal/utf8statetable.h',
        'src/public/compact_lang_det.h',
        'src/public/encodings.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_disabled_warnings': [4267],
        }],
        ['cld2_table_size==0', {
          # Small table sizes
          'sources+': [
            'src/internal/cld2_generated_quadchrome0122_16.cc',
           ],
        }],
        ['cld2_table_size==1', {
          # Medium table sizes
          'sources+': [
            'src/internal/cld2_generated_quadchrome0122_19.cc',
           ],
        }],
        ['cld2_table_size==2', {
          # Large table sizes
          'sources+': [
            'src/internal/cld2_generated_quadchrome0122_2.cc',
           ],
        }],
      ],
    },
    {
      'target_name': 'cld_2',
      'type': 'static_library',
      'include_dirs': [
        'src/internal',
        'src/public',
      ],
      'sources': [
        'src/internal/cld2tablesummary.h',
        'src/internal/cldutil.cc',
        'src/internal/cldutil.h',
        'src/internal/cldutil_shared.cc',
        'src/internal/cldutil_shared.h',
        'src/internal/compact_lang_det.cc',
        'src/internal/compact_lang_det_hint_code.cc',
        'src/internal/compact_lang_det_hint_code.h',
        'src/internal/compact_lang_det_impl.cc',
        'src/internal/compact_lang_det_impl.h',
        'src/internal/debug.h',
        'src/internal/debug_empty.cc',
        'src/internal/fixunicodevalue.cc',
        'src/internal/fixunicodevalue.h',
        'src/internal/generated_distinct_bi_0.cc',
        'src/internal/generated_entities.cc',
        'src/internal/generated_language.cc',
        'src/internal/generated_language.h',
        'src/internal/generated_ulscript.cc',
        'src/internal/generated_ulscript.h',
        'src/internal/getonescriptspan.cc',
        'src/internal/getonescriptspan.h',
        'src/internal/integral_types.h',
        'src/internal/lang_script.cc',
        'src/internal/lang_script.h',
        'src/internal/langspan.h',
        'src/internal/offsetmap.cc',
        'src/internal/offsetmap.h',
        'src/internal/port.h',
        'src/internal/scoreonescriptspan.cc',
        'src/internal/scoreonescriptspan.h',
        'src/internal/stringpiece.h',
        'src/internal/tote.cc',
        'src/internal/tote.h',
        'src/internal/utf8prop_lettermarkscriptnum.h',
        'src/internal/utf8repl_lettermarklower.h',
        'src/internal/utf8scannot_lettermarkspecial.h',
        'src/internal/utf8statetable.cc',
        'src/internal/utf8statetable.h',
        'src/public/compact_lang_det.h',
        'src/public/encodings.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_disabled_warnings': [4267],
        }],
        # Dynamic or vanilla mode
        ['cld2_dynamic==1',
          {
            # Enable dynamic mode, which allows CLD to read its data from a file
            # instead of requiring the data to be linked into the library.
            # You must generate the data file and check it in using the
            # cld2_dynamic_data_tool.
            # build/common.gypi takes care of setting -D CLD2_MODE_DYNAMIC
            'sources+': [
              'src/internal/cld2_dynamic_data.h',
              'src/internal/cld2_dynamic_data.cc',
              'src/internal/cld2_dynamic_data_loader.h',
              'src/internal/cld2_dynamic_data_loader.cc',
            ],
          },
          {
            # Else it's regular mode, so link in all the data blobs directly.
            'sources+': [
              'src/internal/cld2_generated_cjk_compatible.cc',
              'src/internal/cld2_generated_deltaoctachrome0122.cc',
              'src/internal/cld2_generated_distinctoctachrome0122.cc',
              'src/internal/cld_generated_cjk_delta_bi_4.cc',
              'src/internal/cld_generated_cjk_uni_prop_80.cc',
              'src/internal/cld_generated_score_quad_octa_0122_2.cc',
              'src/internal/generated_distinct_bi_0.cc',
            ],
            'conditions': [
              ['cld2_table_size==0', {
                # Small table sizes
                'sources+': [
                  'src/internal/cld2_generated_quadchrome0122_16.cc',
                 ],
              }],
              ['cld2_table_size==1', {
                # Medium table sizes
                'sources+': [
                  'src/internal/cld2_generated_quadchrome0122_19.cc',
                 ],
              }],
              ['cld2_table_size==2', {
                # Large table sizes
                'sources+': [
                  'src/internal/cld2_generated_quadchrome0122_2.cc',
                 ],
              }],
            ],
          },
        ],
      ],
    },
  ],
}
