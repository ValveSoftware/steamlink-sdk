# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/ssl_errors
      'target_name': 'ssl_errors',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base_i18n',
        '../net/net.gyp:net',
        '../ui/base/ui_base.gyp:ui_base',
        'components_strings.gyp:components_strings',
        'network_time',
        'url_formatter/url_formatter.gyp:url_formatter',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'ssl_errors/error_classification.cc',
        'ssl_errors/error_classification.h',
        'ssl_errors/error_info.cc',
        'ssl_errors/error_info.h',
      ]
    }
  ]
}