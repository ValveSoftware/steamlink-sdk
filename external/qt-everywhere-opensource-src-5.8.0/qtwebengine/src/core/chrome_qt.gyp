{
  'variables': {
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome',
    'chrome_spellchecker_sources': [
    '<(DEPTH)/chrome/browser/spellchecker/feedback.cc',
    '<(DEPTH)/chrome/browser/spellchecker/feedback.h',
    '<(DEPTH)/chrome/browser/spellchecker/feedback_sender.cc',
    '<(DEPTH)/chrome/browser/spellchecker/feedback_sender.h',
    '<(DEPTH)/chrome/browser/spellchecker/misspelling.cc',
    '<(DEPTH)/chrome/browser/spellchecker/misspelling.h',
    '<(DEPTH)/chrome/browser/spellchecker/spellcheck_action.cc',
    '<(DEPTH)/chrome/browser/spellchecker/spellcheck_action.h',
    '<(DEPTH)/chrome/browser/spellchecker/spellcheck_custom_dictionary.cc',
    '<(DEPTH)/chrome/browser/spellchecker/spellcheck_custom_dictionary.h',
    '<(DEPTH)/chrome/browser/spellchecker/spellcheck_factory.cc',
    '<(DEPTH)/chrome/browser/spellchecker/spellcheck_factory.h',
    '<(DEPTH)/chrome/browser/spellchecker/spellcheck_host_metrics.cc',
    '<(DEPTH)/chrome/browser/spellchecker/spellcheck_host_metrics.h',
    '<(DEPTH)/chrome/browser/spellchecker/spellcheck_hunspell_dictionary.cc',
    '<(DEPTH)/chrome/browser/spellchecker/spellcheck_hunspell_dictionary.h',
    '<(DEPTH)/chrome/browser/spellchecker/spellcheck_message_filter.cc',
    '<(DEPTH)/chrome/browser/spellchecker/spellcheck_message_filter.h',
    '<(DEPTH)/chrome/browser/spellchecker/spellcheck_message_filter_platform.h',
    '<(DEPTH)/chrome/browser/spellchecker/spellcheck_message_filter_platform_mac.cc',
    '<(DEPTH)/chrome/browser/spellchecker/spellcheck_platform_mac.mm',
    '<(DEPTH)/chrome/browser/spellchecker/spellcheck_service.cc',
    '<(DEPTH)/chrome/browser/spellchecker/spellcheck_service.h',
    '<(DEPTH)/chrome/browser/spellchecker/spelling_service_client.cc',
    '<(DEPTH)/chrome/browser/spellchecker/spelling_service_client.h',
    '<(DEPTH)/chrome/browser/spellchecker/word_trimmer.cc',
    '<(DEPTH)/chrome/browser/spellchecker/word_trimmer.h',
    '<(DEPTH)/chrome/common/common_message_generator.cc',
    '<(DEPTH)/chrome/common/pref_names.cc',
    '<(DEPTH)/chrome/common/pref_names.h',
    '<(DEPTH)/chrome/common/spellcheck_bdict_language.h',
    '<(DEPTH)/chrome/common/spellcheck_common.cc',
    '<(DEPTH)/chrome/common/spellcheck_common.h',
    '<(DEPTH)/chrome/common/spellcheck_marker.h',
    '<(DEPTH)/chrome/common/spellcheck_messages.h',
    '<(DEPTH)/chrome/common/spellcheck_result.h',
    '<(DEPTH)/chrome/renderer/spellchecker/custom_dictionary_engine.cc',
    '<(DEPTH)/chrome/renderer/spellchecker/custom_dictionary_engine.h',
    '<(DEPTH)/chrome/renderer/spellchecker/hunspell_engine.cc',
    '<(DEPTH)/chrome/renderer/spellchecker/hunspell_engine.h',
    '<(DEPTH)/chrome/renderer/spellchecker/platform_spelling_engine.cc',
    '<(DEPTH)/chrome/renderer/spellchecker/platform_spelling_engine.h',
    '<(DEPTH)/chrome/renderer/spellchecker/spellcheck.cc',
    '<(DEPTH)/chrome/renderer/spellchecker/spellcheck.h',
    '<(DEPTH)/chrome/renderer/spellchecker/spellcheck_language.cc',
    '<(DEPTH)/chrome/renderer/spellchecker/spellcheck_language.h',
    '<(DEPTH)/chrome/renderer/spellchecker/spellcheck_provider.cc',
    '<(DEPTH)/chrome/renderer/spellchecker/spellcheck_provider.h',
    '<(DEPTH)/chrome/renderer/spellchecker/spellcheck_worditerator.cc',
    '<(DEPTH)/chrome/renderer/spellchecker/spellcheck_worditerator.h',
    '<(DEPTH)/chrome/renderer/spellchecker/spelling_engine.h',
    ]
  },
  'targets': [
    {
      'target_name': 'chrome_qt',
      'type': 'static_library',
      'dependencies': [
          'chrome_resources',
          '<(chromium_src_dir)/components/components_resources.gyp:components_resources',
          '<(chromium_src_dir)/components/components_strings.gyp:components_strings',
      ],
      'include_dirs': [
        './',
        '<(chromium_src_dir)',
        '<(chromium_src_dir)/skia/config',
        '<(chromium_src_dir)/third_party/skia/include/core',
      ],
      'sources': [
        '<(DEPTH)/chrome/browser/media/desktop_media_list.h',
        '<(DEPTH)/chrome/browser/media/desktop_streams_registry.cc',
        '<(DEPTH)/chrome/browser/media/desktop_streams_registry.h',
        '<(DEPTH)/chrome/browser/profiles/profile.cc',
        '<(DEPTH)/chrome/browser/profiles/profile.h',
        '<(DEPTH)/chrome/common/chrome_switches.cc',
        '<(DEPTH)/chrome/common/chrome_switches.h',
        '<(DEPTH)/components/prefs/testing_pref_store.cc',
        '<(DEPTH)/components/prefs/testing_pref_store.h',
        '<(DEPTH)/extensions/common/constants.cc',
        '<(DEPTH)/extensions/common/constants.h',
        '<(DEPTH)/extensions/common/url_pattern.cc',
        '<(DEPTH)/extensions/common/url_pattern.h',
      ],
      'conditions': [
        ['OS == "win"', {
          # crbug.com/167187 fix size_t to int truncations
          'msvs_disabled_warnings': [4267, ],
        }],
        ['enable_plugins==1', {
          'sources': [
            '<(DEPTH)/chrome/browser/renderer_host/pepper/pepper_flash_clipboard_message_filter.cc',
            '<(DEPTH)/chrome/browser/renderer_host/pepper/pepper_flash_clipboard_message_filter.h',
            '<(DEPTH)/chrome/renderer/pepper/pepper_flash_font_file_host.cc',
            '<(DEPTH)/chrome/renderer/pepper/pepper_flash_font_file_host.h',
            '<(DEPTH)/chrome/renderer/pepper/pepper_shared_memory_message_filter.cc',
            '<(DEPTH)/chrome/renderer/pepper/pepper_shared_memory_message_filter.h',
          ],
        }],
        ['enable_pdf==1', {
          'dependencies': [
            '<(chromium_src_dir)/pdf/pdf.gyp:pdf',
            '<(chromium_src_dir)/components/components.gyp:pdf_renderer',
            '<(chromium_src_dir)/components/components.gyp:pdf_browser',
          ],
        }],
        ['enable_spellcheck==1', {
          'sources': [ '<@(chrome_spellchecker_sources)' ],
          'include_dirs': [
            '<(chromium_src_dir)/third_party/WebKit',
          ],
          'dependencies': [
            '<(chromium_src_dir)/chrome/tools/convert_dict/convert_dict.gyp:convert_dict',
            '<(chromium_src_dir)/third_party/hunspell/hunspell.gyp:hunspell',
            '<(chromium_src_dir)/third_party/icu/icu.gyp:icui18n',
            '<(chromium_src_dir)/third_party/icu/icu.gyp:icuuc',
          ],
          'defines': [
              '__STDC_CONSTANT_MACROS',
              '__STDC_FORMAT_MACROS',
          ],
          'conditions': [
            [ 'OS != "mac"', {
              'sources/': [
                ['exclude', '_mac\\.(cc|cpp|mm?)$'],
              ],
            }],
            ['use_browser_spellchecker==0', {
              'sources!': [
                '<(DEPTH)/chrome/renderer/spellchecker/platform_spelling_engine.cc',
                '<(DEPTH)/chrome/renderer/spellchecker/platform_spelling_engine.h',
                '<(DEPTH)/chrome/browser/spellchecker/spellcheck_message_filter_platform.h',
                '<(DEPTH)/chrome/browser/spellchecker/spellcheck_message_filter_platform_mac.cc',
              ],
            }],
          ],
        }],
        ['enable_basic_printing==1 or enable_print_preview==1', {
          'sources': [
            '<(DEPTH)/chrome/browser/printing/print_job.cc',
            '<(DEPTH)/chrome/browser/printing/print_job.h',
            '<(DEPTH)/chrome/browser/printing/print_job_manager.cc',
            '<(DEPTH)/chrome/browser/printing/print_job_manager.h',
            '<(DEPTH)/chrome/browser/printing/print_job_worker.cc',
            '<(DEPTH)/chrome/browser/printing/print_job_worker.h',
            '<(DEPTH)/chrome/browser/printing/print_job_worker_owner.cc',
            '<(DEPTH)/chrome/browser/printing/print_job_worker_owner.h',
            '<(DEPTH)/chrome/browser/printing/printer_query.cc',
            '<(DEPTH)/chrome/browser/printing/printer_query.h',
          ],
          'dependencies': [
            '<(chromium_src_dir)/mojo/mojo_public.gyp:mojo_cpp_bindings',
          ],
          'include_dirs': [
            '<(chromium_src_dir)/extensions',
          ],
        }],
      ],
    },
    {
      'target_name': 'chrome_resources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'generated_resources',
          'variables': {
            'grit_grd_file': '<(chromium_src_dir)/chrome/app/generated_resources.grd',
          },
          'includes': [ 'resources/grit_action.gypi' ],
        },
        {
          'action_name': 'chromium_strings.grd',
          'variables': {
            'grit_grd_file': '<(chromium_src_dir)/chrome/app/chromium_strings.grd',
          },
          'includes': [ 'resources/grit_action.gypi' ],
        },
        {
          'action_name': 'renderer_resources',
          'variables': {
            'grit_grd_file': '<(chromium_src_dir)/chrome/renderer/resources/renderer_resources.grd',
          },
          'includes': [ 'resources/grit_action.gypi' ],
        },
      ]
    },
  ],
}
