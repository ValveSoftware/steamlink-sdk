# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/win_precompile.gypi',
  ],
  'targets': [
    {
      'target_name': 'cld',
      'type': 'static_library',
      'dependencies': [
        '../icu/icu.gyp:icuuc',
      ],
      'export_dependent_settings': [
        '../icu/icu.gyp:icuuc',
      ],
      'include_dirs': [
        '.',
      ],
      'defines': [
        'CLD_WINDOWS',
      ],
      'sources': [
        'encodings/compact_lang_det/cldutil.cc',
        'encodings/compact_lang_det/cldutil.h',
        'encodings/compact_lang_det/cldutil_dbg.h',
        'encodings/compact_lang_det/cldutil_dbg_empty.cc',
        'encodings/compact_lang_det/compact_lang_det.cc',
        'encodings/compact_lang_det/compact_lang_det.h',
        'encodings/compact_lang_det/compact_lang_det_impl.cc',
        'encodings/compact_lang_det/compact_lang_det_impl.h',
        'encodings/compact_lang_det/ext_lang_enc.cc',
        'encodings/compact_lang_det/ext_lang_enc.h',
        'encodings/compact_lang_det/getonescriptspan.cc',
        'encodings/compact_lang_det/getonescriptspan.h',
        'encodings/compact_lang_det/letterscript_enum.cc',
        'encodings/compact_lang_det/letterscript_enum.h',
        'encodings/compact_lang_det/string_byte_sink.cc',
        'encodings/compact_lang_det/string_byte_sink.h',
        'encodings/compact_lang_det/subsetsequence.cc',
        'encodings/compact_lang_det/subsetsequence.h',
        'encodings/compact_lang_det/tote.cc',
        'encodings/compact_lang_det/tote.h',
        'encodings/compact_lang_det/utf8propjustletter.h',
        'encodings/compact_lang_det/utf8propletterscriptnum.h',
        'encodings/compact_lang_det/utf8scannotjustletterspecial.h',
        'encodings/compact_lang_det/generated/cld_generated_score_quadchrome_0406.cc',
        'encodings/compact_lang_det/generated/compact_lang_det_generated_cjkbis_0.cc',
        'encodings/compact_lang_det/generated/compact_lang_det_generated_ctjkvz.cc',
        'encodings/compact_lang_det/generated/compact_lang_det_generated_deltaoctachrome.cc',
        'encodings/compact_lang_det/generated/compact_lang_det_generated_meanscore.h',
        'encodings/compact_lang_det/generated/compact_lang_det_generated_quadschrome.cc',
        'encodings/compact_lang_det/win/cld_basictypes.h',
        'encodings/compact_lang_det/win/cld_commandlineflags.h',
        'encodings/compact_lang_det/win/cld_google.h',
        'encodings/compact_lang_det/win/cld_htmlutils.h',
        'encodings/compact_lang_det/win/cld_htmlutils_windows.cc',
        'encodings/compact_lang_det/win/cld_logging.h',
        'encodings/compact_lang_det/win/cld_macros.h',
        'encodings/compact_lang_det/win/cld_strtoint.h',
        'encodings/compact_lang_det/win/cld_unicodetext.cc',
        'encodings/compact_lang_det/win/cld_unicodetext.h',
        'encodings/compact_lang_det/win/cld_unilib.h',
        'encodings/compact_lang_det/win/cld_unilib_windows.cc',
        'encodings/compact_lang_det/win/cld_utf.h',
        'encodings/compact_lang_det/win/cld_utf8statetable.cc',
        'encodings/compact_lang_det/win/cld_utf8statetable.h',
        'encodings/compact_lang_det/win/cld_utf8utils.h',
        'encodings/compact_lang_det/win/cld_utf8utils_windows.cc',
        'encodings/internal/encodings.cc',
        'encodings/proto/encodings.pb.h',
        'encodings/public/encodings.h',
        'languages/internal/languages.cc',
        'languages/proto/languages.pb.h',
        'languages/public/languages.h',
        'base/basictypes.h',
        'base/build_config.h',
        'base/casts.h',
        'base/commandlineflags.h',
        'base/global_strip_options.h',
        'base/logging.h',
        'base/macros.h',
        'base/port.h',
        'base/crash.h',
        'base/dynamic_annotations.h',
        'base/scoped_ptr.h',
        'base/stl_decl_msvc.h',
        'base/log_severity.h',
        'base/strtoint.h',
        'base/vlog_is_on.h',
        'base/string_util.h',
        'base/type_traits.h',
        'base/template_util.h',
      ],
      'direct_dependent_settings': {
        'defines': [
          'CLD_WINDOWS',
        ],
        'include_dirs': [
          '.',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'direct_dependent_settings': {
            'defines': [
              'COMPILER_MSVC',
            ],
          },
          # TODO(jschuh): C4267: http://crbug.com/167187 size_t -> int
          'msvs_disabled_warnings': [4005, 4006, 4018, 4244, 4309, 4800, 4267],
        }, {
          'direct_dependent_settings': {
            'defines': [
              'COMPILER_GCC',
            ],
          },
        }],
      ],
    },
  ],
}
