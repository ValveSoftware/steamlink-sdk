# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'include_dirs': [
      'dist/cpp/src',
      'dist/cpp/test',
      # The libphonenumber source (and test code) expects the
      # generated protocol headers to be available with "phonenumbers" include
      # path, e.g. #include "phonenumbers/foo.pb.h".
      '<(SHARED_INTERMEDIATE_DIR)/protoc_out/third_party/libphonenumber',
    ],
    'defines': [
      'I18N_PHONENUMBERS_USE_ALTERNATE_FORMATS=1',
      'I18N_PHONENUMBERS_USE_ICU_REGEXP=1',
    ],
    'conditions': [
      # libphonenumber can only be thread-safe on POSIX platforms. This is ok
      # since Android is the only Chromium port that requires thread-safety.
      # It uses the PhoneNumberUtil singleton in renderer threads as opposed to
      # other platforms that only use it in the browser process (on a single
      # thread). Note that any unsafe use of the library would be caught by a
      # DCHECK.
      ['OS != "android"', {
        'defines': [
          'I18N_PHONENUMBERS_NO_THREAD_SAFETY=1',
        ],
      }],
    ],
  },
  'includes': [
    '../../build/win_precompile.gypi',
  ],
  'targets': [{
    # Build a library without metadata so that we can use it with both testing
    # and production metadata. This library should not be used by clients.
    # GN version: //third_party/libphonenumber:libphonenumber_without_metadata
    'target_name': 'libphonenumber_without_metadata',
    'type': 'static_library',
    'dependencies': [
      '../icu/icu.gyp:icui18n',
      '../icu/icu.gyp:icuuc',
      '../protobuf/protobuf.gyp:protobuf_lite',
    ],
    'sources': [
      'dist/cpp/src/phonenumbers/alternate_format.cc',
      'dist/cpp/src/phonenumbers/asyoutypeformatter.cc',
      'dist/cpp/src/phonenumbers/base/strings/string_piece.cc',
      'dist/cpp/src/phonenumbers/default_logger.cc',
      'dist/cpp/src/phonenumbers/logger.cc',
      'dist/cpp/src/phonenumbers/phonenumber.cc',
      'dist/cpp/src/phonenumbers/phonenumbermatch.cc',
      'dist/cpp/src/phonenumbers/phonenumbermatcher.cc',
      'dist/cpp/src/phonenumbers/phonenumberutil.cc',
      'dist/cpp/src/phonenumbers/regexp_adapter_icu.cc',
      'dist/cpp/src/phonenumbers/regexp_cache.cc',
      'dist/cpp/src/phonenumbers/string_byte_sink.cc',
      'dist/cpp/src/phonenumbers/stringutil.cc',
      'dist/cpp/src/phonenumbers/unicodestring.cc',
      'dist/cpp/src/phonenumbers/utf/rune.c',
      'dist/cpp/src/phonenumbers/utf/unicodetext.cc',
      'dist/cpp/src/phonenumbers/utf/unilib.cc',
      'dist/resources/phonemetadata.proto',
      'dist/resources/phonenumber.proto',
    ],
    'variables': {
      'proto_in_dir': 'dist/resources',
      'proto_out_dir': 'third_party/libphonenumber/phonenumbers',
      'clang_warning_flags': [
        # https://github.com/googlei18n/libphonenumber/pull/741
        '-Wno-unused-private-field',
      ],
    },
    'includes': [ '../../build/protoc.gypi' ],
    'direct_dependent_settings': {
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)/protoc_out/third_party/libphonenumber',
        'dist/cpp/src',
      ],
      'defines': [
        'I18N_PHONENUMBERS_USE_ALTERNATE_FORMATS=1',
        'I18N_PHONENUMBERS_USE_ICU_REGEXP=1',
      ],
      'conditions': [
        ['OS != "android"', {
          'defines': [
            'I18N_PHONENUMBERS_NO_THREAD_SAFETY=1',
          ],
        }],
      ],
    },
    'conditions': [
      ['OS=="win"', {
        'action': [
          '/wo4309',
        ],
      }],
    ],
  },
  {
    # Library used by clients that includes production metadata.
    # GN version: //third_party/libphonenumber
    'target_name': 'libphonenumber',
    'type': 'static_library',
    'dependencies': [
      'libphonenumber_without_metadata',
    ],
    'export_dependent_settings': [
      'libphonenumber_without_metadata',
    ],
    'sources': [
      # Comment next line and uncomment the line after, if complete metadata
      # (with examples) is needed.
      'dist/cpp/src/phonenumbers/lite_metadata.cc',
      #'dist/cpp/src/phonenumbers/metadata.cc',
    ],
  },
  {
    # GN version: //third_party/libphonenumber:libphonenumber_unittests
    'target_name': 'libphonenumber_unittests',
    'type': 'executable',
    'sources': [
      'dist/cpp/src/phonenumbers/test_metadata.cc',
      'dist/cpp/test/phonenumbers/asyoutypeformatter_test.cc',
      'dist/cpp/test/phonenumbers/phonenumbermatch_test.cc',
      'dist/cpp/test/phonenumbers/phonenumbermatcher_test.cc',
      'dist/cpp/test/phonenumbers/phonenumberutil_test.cc',
      'dist/cpp/test/phonenumbers/regexp_adapter_test.cc',
      'dist/cpp/test/phonenumbers/stringutil_test.cc',
      'dist/cpp/test/phonenumbers/test_util.cc',
      'dist/cpp/test/phonenumbers/unicodestring_test.cc',
    ],
    'dependencies': [
      '../icu/icu.gyp:icui18n',
      '../icu/icu.gyp:icuuc',
      '../../base/base.gyp:run_all_unittests',
      '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      '../../testing/gmock.gyp:gmock',
      '../../testing/gtest.gyp:gtest',
      'libphonenumber_without_metadata',
    ],
    'variables': {
      'clang_warning_flags': [
        # https://github.com/googlei18n/libphonenumber/pull/741
        '-Wno-unused-private-field',
      ],
    },
    'conditions': [
      ['OS=="win"', {
        'action': [
          '/wo4309',
        ],
      }],
      # libphonenumber needs to fix their ODR violations. http://crbug.com/456021
      ['OS=="linux"', {
        'ldflags!': [
          '-Wl,--detect-odr-violations',
        ],
      }],
    ],
  }]
}
