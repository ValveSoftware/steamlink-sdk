# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'remoting_localize_path': 'tools/build/remoting_localize.py',

    'webapp_locale_dir': '<(SHARED_INTERMEDIATE_DIR)/remoting/webapp/_locales',

    'remoting_locales': [
      # Note: list duplicated in GN build. See //remoting/remoting_locales.gni
      'am', 'ar', 'bg', 'bn', 'ca', 'cs', 'da', 'de', 'el', 'en', 'en-GB', 'es',
      'es-419', 'et', 'fa', 'fake-bidi', 'fi', 'fil', 'fr', 'gu', 'he', 'hi',
      'hr', 'hu', 'id', 'it', 'ja', 'kn', 'ko', 'lt', 'lv', 'ml', 'mr', 'ms',
      'nb', 'nl', 'pl', 'pt-BR', 'pt-PT', 'ro', 'ru', 'sk', 'sl', 'sr', 'sv',
      'sw', 'ta', 'te', 'th', 'tr', 'uk', 'vi', 'zh-CN', 'zh-TW',
    ],
    'remoting_webapp_locale_files': [
      # Build the list of .json files generated from remoting_strings.grd.
      '<!@pymod_do_main(remoting_localize --locale_output '
          '"<(webapp_locale_dir)/@{json_suffix}/messages.json" '
          '--print_only <(remoting_locales))',
    ],
  },  # variables
}
