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
  'targets': [
  {
    'target_name': 'qtwebengine_resources',
    'type': 'none',
    'dependencies': [
        '<(chromium_src_dir)/content/app/strings/content_strings.gyp:content_strings',
        '<(chromium_src_dir)/content/browser/devtools/devtools_resources.gyp:devtools_resources',
        '<(chromium_src_dir)/components/components_resources.gyp:components_resources',
        '<(chromium_src_dir)/components/components_strings.gyp:components_strings',
        '<(chromium_src_dir)/third_party/WebKit/public/blink_resources.gyp:blink_resources',
        '<(qtwebengine_root)/src/core/chrome_qt.gyp:chrome_resources',
    ],
    'actions' : [
      {
        'action_name': 'repack_resources',
        'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/components/components_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/webui_resources.pak',
            ],
            'pak_outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/repack/qtwebengine_resources.pak'
            ]
        },
        'includes': [ 'repack_resources.gypi' ],
      },
      {
        'action_name': 'repack_resources_100_percent',
        'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/components/components_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/app/resources/content_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_image_resources_100_percent.pak',
            ],
            'pak_outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/repack/qtwebengine_resources_100p.pak'
            ]
        },
        'includes': [ 'repack_resources.gypi' ],
      },
      {
        'action_name': 'repack_resources_200_percent',
        'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_200_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/components/components_resources_200_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/app/resources/content_resources_200_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources_200_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_image_resources_200_percent.pak',
            ],
            'pak_outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/repack/qtwebengine_resources_200p.pak'
            ]
        },
        'includes': [ 'repack_resources.gypi' ],
      },
      {
        'action_name': 'repack_resources_devtools',
        'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/blink/devtools_resources.pak',
            ],
            'pak_outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/repack/qtwebengine_devtools_resources.pak'
            ]
        },
        'includes': [ 'repack_resources.gypi' ],
      },
      {
        'action_name': 'repack_locales',
        'includes': [ 'repack_locales.gypi' ],
      },
    ],
  }
  ]
}
