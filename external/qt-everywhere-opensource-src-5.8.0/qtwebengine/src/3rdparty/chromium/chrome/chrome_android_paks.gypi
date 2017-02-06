# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'includes': ['../build/android/v8_external_startup_data_arch_suffix.gypi'],
  'variables': {
    'chrome_android_pak_output_folder': '<@(PRODUCT_DIR)/assets/<(package_name)',
    'chrome_android_pak_input_resources': [
      '<(PRODUCT_DIR)/resources.pak',
      '<(PRODUCT_DIR)/chrome_100_percent.pak',
    ],
    'chrome_android_pak_locale_resources': [
      '<(PRODUCT_DIR)/locales/am.pak',
      '<(PRODUCT_DIR)/locales/ar.pak',
      '<(PRODUCT_DIR)/locales/bg.pak',
      '<(PRODUCT_DIR)/locales/ca.pak',
      '<(PRODUCT_DIR)/locales/cs.pak',
      '<(PRODUCT_DIR)/locales/da.pak',
      '<(PRODUCT_DIR)/locales/de.pak',
      '<(PRODUCT_DIR)/locales/el.pak',
      '<(PRODUCT_DIR)/locales/en-GB.pak',
      '<(PRODUCT_DIR)/locales/en-US.pak',
      '<(PRODUCT_DIR)/locales/es.pak',
      '<(PRODUCT_DIR)/locales/es-419.pak',
      '<(PRODUCT_DIR)/locales/fa.pak',
      '<(PRODUCT_DIR)/locales/fi.pak',
      '<(PRODUCT_DIR)/locales/fil.pak',
      '<(PRODUCT_DIR)/locales/fr.pak',
      '<(PRODUCT_DIR)/locales/he.pak',
      '<(PRODUCT_DIR)/locales/hi.pak',
      '<(PRODUCT_DIR)/locales/hr.pak',
      '<(PRODUCT_DIR)/locales/hu.pak',
      '<(PRODUCT_DIR)/locales/id.pak',
      '<(PRODUCT_DIR)/locales/it.pak',
      '<(PRODUCT_DIR)/locales/ja.pak',
      '<(PRODUCT_DIR)/locales/ko.pak',
      '<(PRODUCT_DIR)/locales/lt.pak',
      '<(PRODUCT_DIR)/locales/lv.pak',
      '<(PRODUCT_DIR)/locales/nb.pak',
      '<(PRODUCT_DIR)/locales/nl.pak',
      '<(PRODUCT_DIR)/locales/pl.pak',
      '<(PRODUCT_DIR)/locales/pt-BR.pak',
      '<(PRODUCT_DIR)/locales/pt-PT.pak',
      '<(PRODUCT_DIR)/locales/ro.pak',
      '<(PRODUCT_DIR)/locales/ru.pak',
      '<(PRODUCT_DIR)/locales/sk.pak',
      '<(PRODUCT_DIR)/locales/sl.pak',
      '<(PRODUCT_DIR)/locales/sr.pak',
      '<(PRODUCT_DIR)/locales/sv.pak',
      '<(PRODUCT_DIR)/locales/sw.pak',
      '<(PRODUCT_DIR)/locales/th.pak',
      '<(PRODUCT_DIR)/locales/tr.pak',
      '<(PRODUCT_DIR)/locales/uk.pak',
      '<(PRODUCT_DIR)/locales/vi.pak',
      '<(PRODUCT_DIR)/locales/zh-CN.pak',
      '<(PRODUCT_DIR)/locales/zh-TW.pak',
    ],
    'chrome_android_pak_output_resources': [
      '<(chrome_android_pak_output_folder)/resources.pak',
      '<(chrome_android_pak_output_folder)/chrome_100_percent.pak',
    ],
    'conditions': [
      ['icu_use_data_file_flag==1', {
        'chrome_android_pak_input_resources': [
          '<(PRODUCT_DIR)/icudtl.dat',
        ],
        'chrome_android_pak_output_resources': [
          '<(chrome_android_pak_output_folder)/icudtl.dat',
        ],
      }],
      ['v8_use_external_startup_data==1', {
        'chrome_android_pak_output_resources': [
          '<(chrome_android_pak_output_folder)/natives_blob_<(arch_suffix).bin',
          '<(chrome_android_pak_output_folder)/snapshot_blob_<(arch_suffix).bin',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'chrome_android_paks_copy',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/chrome/chrome_resources.gyp:packed_resources',
        '<(DEPTH)/chrome/chrome_resources.gyp:packed_extra_resources',
      ],
      'variables': {
        'dest_path': '<(chrome_android_pak_output_folder)',
        'src_files': [
          '<@(chrome_android_pak_input_resources)',
        ],
        'clear': 1,
        'conditions': [
          ['v8_use_external_startup_data==1', {
             'renaming_sources': [
               '<(PRODUCT_DIR)/snapshot_blob.bin',
               '<(PRODUCT_DIR)/natives_blob.bin',
             ],
             'renaming_destinations': [
               'snapshot_blob_<(arch_suffix).bin',
               'natives_blob_<(arch_suffix).bin',
             ],
          }],
        ],
      },
      'includes': ['../build/android/copy_ex.gypi'],
    },
  ],
}

