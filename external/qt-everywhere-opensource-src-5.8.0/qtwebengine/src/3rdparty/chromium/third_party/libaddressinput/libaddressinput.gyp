# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'includes': ['src/cpp/libaddressinput.gypi'],
  'variables': {
    'libaddressinput_test_data_dir%': 'src/third_party/libaddressinput/src/testdata',
    'libaddressinput_util_files': [
      'src/cpp/src/address_data.cc',
      'src/cpp/src/address_field.cc',
      'src/cpp/src/address_field_util.cc',
      'src/cpp/src/address_formatter.cc',
      'src/cpp/src/address_metadata.cc',
      'src/cpp/src/address_ui.cc',
      'src/cpp/src/format_element.cc',
      'src/cpp/src/language.cc',
      'src/cpp/src/localization.cc',
      'src/cpp/src/lookup_key.cc',
      'src/cpp/src/region_data_constants.cc',
      'src/cpp/src/rule.cc',
      'src/cpp/src/util/cctype_tolower_equal.cc',
      'src/cpp/src/util/json.cc',
      'src/cpp/src/util/string_split.cc',
      'src/cpp/src/util/string_util.cc',
    ],
  },
  'targets': [
    {
      'target_name': 'libaddressinput_strings',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/libaddressinput/',
        'grit_grd_file': '../../chrome/app/address_input_strings.grd',
      },
      'actions': [
        {
          'action_name': 'libaddressinput_strings',
          'variables': {
          },
          'includes': [
            '../../build/grit_action.gypi',
          ],
        },
      ],
      'direct_dependent_settings': {
        # Files in libaddressinput include the grit-generated en_messages.cc
        # without knowing its path.
        'include_dirs': [
          '<(grit_out_dir)',
        ],
      },
    },
    {
      'target_name': 'libaddressinput_util',
      'type': 'static_library',
      'sources': [
        '<@(libaddressinput_util_files)',
        'chromium/addressinput_util.cc',
        'chromium/json.cc',
      ],
      'sources!': [
        'src/cpp/src/util/json.cc',
      ],
      'include_dirs': [
        'chromium/override/',
        'src/cpp/include/',
      ],
      'defines': [
        'I18N_ADDRESSINPUT_USE_BASICTYPES_OVERRIDE=1',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'chromium/override/',
          'src/cpp/include/',
        ],
        'defines': [
          'I18N_ADDRESSINPUT_USE_BASICTYPES_OVERRIDE=1',
        ],
      },
      'dependencies': [
        '../../base/base.gyp:base',
        '../re2/re2.gyp:re2',
        'libaddressinput_strings',
      ],
      'export_dependent_settings': [
        'libaddressinput_strings',
      ],
    },
    {
      'target_name': 'libaddressinput',
      'type': 'static_library',
      'sources': [
        '<@(libaddressinput_files)',
        'chromium/chrome_address_validator.cc',
        'chromium/chrome_metadata_source.cc',
        'chromium/chrome_storage_impl.cc',
        'chromium/fallback_data_store.cc',
        'chromium/input_suggester.cc',
        'chromium/string_compare.cc',
        'chromium/trie.cc',
      ],
      'sources!': [
        '<@(libaddressinput_util_files)',
        'src/cpp/src/util/string_compare.cc',
      ],
      'direct_dependent_settings': {
        'defines': [
          'I18N_ADDRESS_VALIDATION_DATA_URL="https://chromium-i18n.appspot.com/ssl-aggregate-address/"',
        ],
      },
      'dependencies': [
        '../../base/base.gyp:base',
        '../../components/prefs/prefs.gyp:prefs',
        '../../net/net.gyp:net',
        '../icu/icu.gyp:icui18n',
        '../icu/icu.gyp:icuuc',
        '../re2/re2.gyp:re2',
        'libaddressinput_util',
      ],
      'export_dependent_settings': [
        'libaddressinput_util',
      ],
    },
    {
      'target_name': 'libaddressinput_unittests',
      'type': '<(gtest_target_type)',
      'sources': [
        '<@(libaddressinput_test_files)',
        'chromium/addressinput_util_unittest.cc',
        'chromium/chrome_address_validator_unittest.cc',
        'chromium/chrome_metadata_source_unittest.cc',
        'chromium/chrome_storage_impl_unittest.cc',
        'chromium/fallback_data_store_unittest.cc',
        'chromium/storage_test_runner.cc',
        'chromium/string_compare_unittest.cc',
        'chromium/trie_unittest.cc',
      ],
      'defines': [
        'TEST_DATA_DIR="<(libaddressinput_test_data_dir)"',
      ],
      'include_dirs': [
        '../../',
        'src/cpp/src/',
      ],
      'dependencies': [
        '../../base/base.gyp:run_all_unittests',
        '../../components/prefs/prefs.gyp:prefs',
        '../../net/net.gyp:net_test_support',
        '../../testing/gtest.gyp:gtest',
        'libaddressinput',
        'libaddressinput_util',
      ],
    },
  ],
}
