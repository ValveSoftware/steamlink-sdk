# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/url_formatter
      'target_name': 'url_formatter',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../net/net.gyp:net',
        '../../third_party/icu/icu.gyp:icui18n',
        '../../third_party/icu/icu.gyp:icuuc',
        '../../url/url.gyp:url_lib',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'elide_url.cc',
        'elide_url.h',
        'url_fixer.cc',
        'url_fixer.h',
        'url_formatter.cc',
        'url_formatter.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],

      'conditions': [
        ['OS != "android"', {
          'dependencies': [
            '../../ui/gfx/gfx.gyp:gfx',
          ]
        }],
      ],
    },
  ],
}
