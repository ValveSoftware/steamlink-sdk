{
  'variables': {
    # Used in repack_locales
    'locales': [
      'am', 'ar', 'bg', 'bn', 'ca', 'cs', 'da', 'de', 'el', 'en-GB',
      'en-US', 'es-419', 'es', 'et', 'fa', 'fi', 'fil', 'fr', 'gu', 'he',
      'hi', 'hr', 'hu', 'id', 'it', 'ja', 'kn', 'ko', 'lt', 'lv',
      'ml', 'mr', 'ms', 'nb', 'nl', 'pl', 'pt-BR', 'pt-PT', 'ro', 'ru',
      'sk', 'sl', 'sr', 'sv', 'sw', 'ta', 'te', 'th', 'tr', 'uk',
      'vi', 'zh-CN', 'zh-TW',
    ],
    'locale_files': ['<!@pymod_do_main(repack_locales -o -p <(OS) -s <(SHARED_INTERMEDIATE_DIR) -x <(SHARED_INTERMEDIATE_DIR) <(locales))'],
    'qt_install_data%': '',
    'qt_install_translations%': '',
  },
  'dependencies': [
      '<(chromium_src_dir)/webkit/webkit_resources.gyp:webkit_strings',
      '<(chromium_src_dir)/webkit/webkit_resources.gyp:webkit_resources',
      '<(chromium_src_dir)/content/browser/devtools/devtools_resources.gyp:devtools_resources',
      '../chrome_qt.gyp:chrome_resources',
  ],
  'targets': [
  {
    'target_name': 'qtwebengine_resources',
    'type': 'none',
    'actions' : [
      {
        'action_name': 'repack_resources',
        'includes': [ 'repack_resources.gypi' ],
      },
      {
        'action_name': 'repack_locales',
        'includes': [ 'repack_locales.gypi' ],
      },
    ],
    'conditions': [
      ['qt_install_data != ""', {
        'copies': [
          {
            'destination': '<(qt_install_data)',
            'files': [ '<(SHARED_INTERMEDIATE_DIR)/repack/qtwebengine_resources.pak' ],
          },
        ],
      }],
      ['qt_install_translations != ""', {
        'copies': [
          {
            'destination': '<(qt_install_translations)/qtwebengine_locales',
            'files': [ '<@(locale_files)' ],
          },
        ],
      }],
    ],
  }
  ]
}
