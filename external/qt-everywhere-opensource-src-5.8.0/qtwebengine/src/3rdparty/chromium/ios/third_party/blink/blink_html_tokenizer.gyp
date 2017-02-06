# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      # GN version: //ios/third_party/blink:html_tokenizer
      'target_name': 'blink_html_tokenizer',
      'type': 'static_library',
      'dependencies': [
        '../../../base/base.gyp:base',
      ],
      'include_dirs': [
        '../../..',
      ],
      'sources': [
        'src/html_character_provider.h',
        'src/html_input_stream_preprocessor.h',
        'src/html_markup_tokenizer_inlines.h',
        'src/html_token.h',
        'src/html_token.mm',
        'src/html_tokenizer.h',
        'src/html_tokenizer.mm',
        'src/html_tokenizer_adapter.h',
      ],
    },
  ],
}
