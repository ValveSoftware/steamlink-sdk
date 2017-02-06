# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/quirks
      'target_name': 'quirks',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../components/prefs/prefs.gyp:prefs',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'version_info',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'QUIRKS_IMPLEMENTATION',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'quirks/pref_names.cc',
        'quirks/pref_names.h',
        'quirks/quirks_client.cc',
        'quirks/quirks_client.h',
        'quirks/quirks_export.h',
        'quirks/quirks_manager.cc',
        'quirks/quirks_manager.h',
      ],
    },
  ],
}
