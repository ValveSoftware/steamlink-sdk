# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'include_dirs': [
      'src',
      'src/test',
      # The libphonenumber source (and test code) expects the
      # generated protocol headers to be available with "phonenumbers" include
      # path, e.g. #include "phonenumbers/foo.pb.h".
      '<(SHARED_INTERMEDIATE_DIR)/protoc_out/third_party/libphonenumber',
    ],
    'defines': [
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
    'target_name': 'libphonenumber_without_metadata',
    'type': 'static_library',
    'dependencies': [
      '../icu/icu.gyp:icui18n',
      '../icu/icu.gyp:icuuc',
      '../protobuf/protobuf.gyp:protobuf_lite',
    ],
    'sources': [
      'src/phonenumbers/asyoutypeformatter.cc',
      'src/phonenumbers/base/strings/string_piece.cc',
      'src/phonenumbers/default_logger.cc',
      'src/phonenumbers/logger.cc',
      'src/phonenumbers/phonenumber.cc',
      'src/phonenumbers/phonenumbermatch.cc',
      'src/phonenumbers/phonenumbermatcher.cc',
      'src/phonenumbers/phonenumberutil.cc',
      'src/phonenumbers/regexp_adapter_icu.cc',
      'src/phonenumbers/regexp_cache.cc',
      'src/phonenumbers/string_byte_sink.cc',
      'src/phonenumbers/stringutil.cc',
      'src/phonenumbers/unicodestring.cc',
      'src/phonenumbers/utf/rune.c',
      'src/phonenumbers/utf/unicodetext.cc',
      'src/phonenumbers/utf/unilib.cc',
      'src/resources/phonemetadata.proto',
      'src/resources/phonenumber.proto',
    ],
    'variables': {
      'proto_in_dir': 'src/resources',
      'proto_out_dir': 'third_party/libphonenumber/phonenumbers',
    },
    'includes': [ '../../build/protoc.gypi' ],
    'direct_dependent_settings': {
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)/protoc_out/third_party/libphonenumber',
        'src',
      ],
      'defines': [
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
      'src/phonenumbers/lite_metadata.cc',
      #'src/phonenumbers/metadata.cc',
    ],
  },
  {
    'target_name': 'libphonenumber_unittests',
    'type': 'executable',
    'sources': [
      'src/phonenumbers/test_metadata.cc',
      'src/test/phonenumbers/asyoutypeformatter_test.cc',
      'src/test/phonenumbers/phonenumbermatch_test.cc',
      'src/test/phonenumbers/phonenumbermatcher_test.cc',
      'src/test/phonenumbers/phonenumberutil_test.cc',
      'src/test/phonenumbers/regexp_adapter_test.cc',
      'src/test/phonenumbers/stringutil_test.cc',
      'src/test/phonenumbers/test_util.cc',
      'src/test/phonenumbers/unicodestring_test.cc',
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
    'conditions': [
      ['OS=="win"', {
        'action': [
          '/wo4309',
        ],
      }],
    ],
  }]
}
