# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/error_page/common
      'target_name': 'error_page_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        '../ui/base/ui_base.gyp:ui_base',
        'components_strings.gyp:components_strings',
        'url_formatter/url_formatter.gyp:url_formatter',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'error_page/common/error_page_params.cc',
        'error_page/common/error_page_params.h',
        'error_page/common/error_page_switches.cc',
        'error_page/common/error_page_switches.h',
        'error_page/common/localized_error.cc',
        'error_page/common/localized_error.h',
        'error_page/common/net_error_info.cc',
        'error_page/common/net_error_info.h',
      ],
    },
  ],
  'conditions': [
    ['OS != "ios"', {
      'targets': [
        {
          # GN version: //components/error_page/renderer
          'target_name': 'error_page_renderer',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../components/components_strings.gyp:components_strings',
            '../content/content.gyp:content_common',
            '../net/net.gyp:net',
            '../third_party/WebKit/public/blink.gyp:blink',
            '../ui/base/ui_base.gyp:ui_base',
            '../url/url.gyp:url_lib',
            'error_page_common',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'error_page/renderer/net_error_helper_core.cc',
            'error_page/renderer/net_error_helper_core.h',
          ],
        }
      ],
    }],
  ],
}
